#pragma once

#include <adtffiltersdk/adtf_filtersdk.h>

#include <QWidget>
#include <QImageIOHandler> 

using namespace adtf::base;
using namespace adtf::streaming;
using namespace adtf::filter;

class qt5adtf3_image_compressor : public cFilter
{
public:
    ADTF_CLASS_ID_NAME(qt5adtf3_image_compressor,
        "compressimage.filter.ad.cid",
        "Image sample / video stream compressor");

    // It is a constructor - yeah 
    qt5adtf3_image_compressor();
    ~qt5adtf3_image_compressor() = default;

    tResult ProcessInput(ISampleReader* pReader,
        const iobject_ptr<const ISample>& pSample) override;

    tResult AcceptType(ISampleReader* pReader, const iobject_ptr<const IStreamType>& pType) override;

    // compressed sample is still an image so we use adtf/image stream type
    // .. and extend some elements
    struct stream_meta_type_image_compressed : public stream_meta_type_image
    {
        static constexpr const tChar *const MetaTypeName = "adtf/image/compressed";
        static constexpr const tChar *const CompressionType = "compression_type";
        static tVoid SetProperties(const adtf::ucom::iobject_ptr<adtf::base::IProperties>& pProperties)
        {
            pProperties->SetProperty(adtf::base::property<adtf_util::cString>(CompressionType, ""));
            stream_meta_type_image::SetProperties(pProperties);
        }
    };

private:
    ISampleWriter* m_pCompressedWriter;

    property_variable<adtf::util::cString> m_encoder_format = "png";    // png - lossless and well supported
    property_variable<tInt> m_encoder_quality = 85;                     // encoder specific - 85 seems good for png
    property_variable<tFloat> m_encoder_gamma = 1.0;                    // encoder specific value
    property_variable<tBool> m_encoder_optimized_write = false;         // encoder specific value
    property_variable<tBool> m_encoder_progressive_scan_write = false;  // encoder specific value
    property_variable<tInt> m_encoder_compression = -1;                 // encoder specific value; -1 for png
    property_variable<adtf::util::cString> m_encoder_additional_text = "";  // you can add some more infos in file header
    property_variable<QImageIOHandler::Transformation> m_encoder_transformation = QImageIOHandler::Transformation::TransformationNone;

    property_variable<tBool> m_postprocess_to_base64 = false;           // convert compressed data to Base64

    tStreamImageFormat m_sCurrentInputFormat;
    QImage::Format m_eCurrentQtFormat;


    // only tested formates, feel free to extend
    const std::map<std::string, QImage::Format> image2qt_mapping =
    {
        {stream_image_format::GREYSCALE_8::FormatName, QImage::Format_Grayscale8},
        {stream_image_format::RGB_24::FormatName, QImage::Format_RGB888},
    };
};
