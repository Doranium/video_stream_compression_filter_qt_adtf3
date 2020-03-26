
#include "qt_image_sample_compressor.h"

#include <QImageWriter>
#include <QBuffer>
#include <QImage>

using namespace adtf::ucom;

ADTF_PLUGIN("VideoImageCompressorADTFqt", qt5adtf3_image_compressor)


qt5adtf3_image_compressor::qt5adtf3_image_compressor()
{
    CreateInputPin("video", true, false);
    m_pCompressedWriter = CreateOutputPin("compressed"); 

    SetDescription("Use this filter to compress bare, uncompressed ADTF video streams in well known formats.");

    RegisterPropertyVariable("Quality", m_encoder_quality);
    RegisterPropertyVariable("Gamma", m_encoder_gamma);
    RegisterPropertyVariable("Optimized Write", m_encoder_optimized_write);
    RegisterPropertyVariable("Progressive Scan Write", m_encoder_progressive_scan_write);
    RegisterPropertyVariable("Compression Type", m_encoder_compression);
    RegisterPropertyVariable("Additional Text", m_encoder_additional_text);
    RegisterPropertyVariable("Save data in Base64", m_postprocess_to_base64);

    // not all qt installations have the same valid image encode output formats
    // identify supported compression types and send to log(view)
    std::string supported_formats;
    for (auto imageformat : QImageWriter::supportedImageFormats())
        supported_formats.append(imageformat.toStdString()).append(" ");
    LOG_INFO("Supported compression formats: ", supported_formats.c_str());
    m_encoder_format.SetDescription("Specify desired image type e.g jpg, png, tif, bmp, webp.");
    RegisterPropertyVariable("Output Encoder Format", m_encoder_format);

    using t = QImageIOHandler::Transformation;
    m_encoder_transformation.SetValueList({
        {t::TransformationNone , "None"},
        {t::TransformationMirror ,"Mirror horizontally"},
        {t::TransformationFlip ,"Mirror vertically"},
        {t::TransformationRotate90 ,"Rotate 90"},
        {t::TransformationRotate180 ,"Rotate 180"},
        {t::TransformationRotate270 ,"Rotate 270"},
        {t::TransformationFlipAndRotate90 ,"Mirror vertically + Rotate 90"},
        {t::TransformationMirrorAndRotate90 ,"Mirror horizontally + Rotate 90"},
        });
    m_encoder_transformation.SetDescription("Transform image before compression");
    RegisterPropertyVariable("Encoder Transformation", m_encoder_transformation);
}


tResult qt5adtf3_image_compressor::ProcessInput(ISampleReader* /*pReader*/,
    const iobject_ptr<const ISample>& pSample)
{
    if (m_eCurrentQtFormat == QImage::Format_Invalid) RETURN_ERROR(ERR_UNKNOWN_FORMAT);     // just in case of unknown format

    adtf::ucom::object_ptr_shared_locked<const adtf::streaming::ISampleBuffer> pBuffer;
    pSample->Lock(pBuffer);

    // check if sample has enough data for a correct image
    // todo: is data alignment correct ?
    auto bytes_per_pixel = stream_image_format_get_generic_pixel_size(m_sCurrentInputFormat);
    auto bytes_per_line = bytes_per_pixel / 8 * m_sCurrentInputFormat.m_ui32Width;
    bytes_per_line = ((bytes_per_line + 3) / 4) * 4;    // QImage requires 4 byte / 32 bit alignment per line
    auto bytes_per_image = bytes_per_line * m_sCurrentInputFormat.m_ui32Height;
    auto check_size = pBuffer->GetSize();
    if (bytes_per_image > check_size) RETURN_ERROR(ERR_OUT_OF_RANGE);

    // data seems ok, now generate a qt image (uncompressed)
    adtf::util::cMemoryBlock current_image_store;
    pBuffer->Read(adtf_memory_intf(current_image_store));
    QImage new_qt_image(static_cast<const uchar*>(current_image_store.GetPtr()),
        m_sCurrentInputFormat.m_ui32Width,
        m_sCurrentInputFormat.m_ui32Height,
        static_cast<tInt>(bytes_per_line),
        m_eCurrentQtFormat);

    // qt image can fail, e.g. on wrong alignment, but should be already checked 
    if (new_qt_image.isNull()
        || !new_qt_image.size().isValid()
        || QImage::Format::Format_Invalid == new_qt_image.format())
        RETURN_ERROR(ERR_UNEXPECTED);


    // compress image in memory using qt
    // needs some configuration first
    // always create and configure NEW writer because properties can change on runtime
    QBuffer qt_buffer;
    adtf::util::cString encoder_format = m_encoder_format;
    encoder_format.Trim();
    QImageWriter writer(&qt_buffer, encoder_format.GetPtr());

    // there are a lot of possible parameters, most are encoder dependent
    writer.setFormat(encoder_format.GetPtr());
    writer.setQuality(m_encoder_quality);
    writer.setGamma(m_encoder_gamma);
    writer.setOptimizedWrite(m_encoder_optimized_write);
    writer.setProgressiveScanWrite(m_encoder_progressive_scan_write);
    writer.setText("tTimeStamp", QString::number(pSample->GetTime()));
    writer.setCompression(m_encoder_compression);
    QImageIOHandler::Transformation encoder_transformation = m_encoder_transformation;
    writer.setTransformation(encoder_transformation);
    if (!m_encoder_additional_text->IsEmpty())
        writer.setText("AdditionalText", adtf::util::cString(m_encoder_additional_text).GetPtr());
    // writer.setSubType(());  // example case needed first
    // configuration done

    // encode data
    if (!writer.canWrite()) RETURN_AND_LOG_ERROR_STR(ERR_FAILED, writer.errorString().toLatin1().data());
    if (!writer.write(new_qt_image)) RETURN_AND_LOG_ERROR_STR(ERR_FAILED, writer.errorString().toLatin1().data());

    // post processing
    QByteArray compressed_data = qt_buffer.data();
    if (m_postprocess_to_base64) compressed_data = compressed_data.toBase64();
    if (compressed_data.isEmpty()) RETURN_ERROR(ERR_EMPTY);

    // prepare and send out data sample
    const void* compressed_data_ptr = compressed_data.data();
    const auto compresed_data_size = compressed_data.size();

    adtf::ucom::object_ptr<adtf::streaming::ant::ISample> write_sample;
    adtf::ucom::object_ptr_locked<adtf::streaming::ISampleBuffer> write_buffer;
    adtf::streaming::ant::alloc_sample(write_sample, pSample->GetTime());
    write_sample->WriteLock(write_buffer, compresed_data_size);
    std::memcpy(write_buffer->GetPtr(), compressed_data_ptr, compresed_data_size);

    RETURN_IF_FAILED(m_pCompressedWriter->Write(write_sample));
    RETURN_IF_FAILED(m_pCompressedWriter->ManualTrigger());
    RETURN_NOERROR;
}


tResult qt5adtf3_image_compressor::AcceptType(ISampleReader* /*pReader*/, const iobject_ptr<const IStreamType>& pType)
{
    // check if stream type is image and error if not 
    if (!pType.Get()) RETURN_ERROR(ERR_INVALID_TYPE);
    RETURN_IF_FAILED(get_stream_type_image_format(m_sCurrentInputFormat, *pType.Get()));

    // JavaScript filter has a converter class for ADTF stream image formats to qt formats
    // .. but not yet so we have to convert manually
    // converting map is in header file
    try {
        m_eCurrentQtFormat = image2qt_mapping.at(m_sCurrentInputFormat.m_strFormatName.GetPtr());
    }
    catch (std::out_of_range) {
        m_eCurrentQtFormat = QImage::Format_Invalid;
        RETURN_ERROR(ERR_UNKNOWN_FORMAT);
    }

    // set output compressed stream type
    // similar to input type plus some specializations
    object_ptr<IStreamType> pTypeOutput = make_object_ptr<cStreamType>(stream_meta_type_image_compressed());
    adtf::util::cString write_format(m_encoder_format);
    write_format.Trim();
    set_property(*pTypeOutput, stream_meta_type_image_compressed::CompressionType, write_format.GetPtr());
    set_property(*pTypeOutput, stream_meta_type_image::FormatName, m_sCurrentInputFormat.m_strFormatName.GetPtr());
    set_property(*pTypeOutput, stream_meta_type_image::PixelHeight, m_sCurrentInputFormat.m_ui32Height);
    set_property(*pTypeOutput, stream_meta_type_image::PixelWidth, m_sCurrentInputFormat.m_ui32Width);
    m_pCompressedWriter->ChangeType(pTypeOutput);

    RETURN_NOERROR;
}
