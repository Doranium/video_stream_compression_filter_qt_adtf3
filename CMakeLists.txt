cmake_minimum_required(VERSION 3.10.0)

project(image_video_compression_qt VERSION 0.0.1)

find_package(ADTF COMPONENTS filtersdk ui)

adtf_use_qt( Widgets Core Gui )

adtf_add_filter(${PROJECT_NAME}
                qt_image_sample_compressor.h
                qt_image_sample_compressor.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE 
                        adtf::filtersdk
                        adtf::ui
                        Qt5::Core
                        Qt5::Widgets
                        Qt5::Gui
                        )

# Adds the INSTALL project to the Visual Studio solution, which when build
# copies our filter to the subdirectory given as the second argument into ${CMAKE_INSTALL_PREFIX}
adtf_install_filter(${PROJECT_NAME} bin)

# Generate a plugindescription for our filter
adtf_create_plugindescription(TARGET ${PROJECT_NAME} PLUGIN_SUBDIR "bin")

# ignore linker warning 4099 for missing PDBs
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
