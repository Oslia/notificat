# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
# idf_build_set_property(MINIMAL_BUILD ON)
project(notificat)

idf_component_get_property(fatfs_lib fatfs COMPONENT_LIB)
idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
idf_component_get_property(freetype_lib espressif__freetype COMPONENT_LIB)
idf_component_get_property(png_lib espressif__libpng COMPONENT_LIB)
target_link_libraries(${lvgl_lib} PUBLIC ${freetype_lib} ${png_lib})
target_compile_definitions(${lvgl_lib} PUBLIC LV_ATTRIBUTE_LARGE_RAM_ARRAY=EXT_RAM_BSS_ATTR)


target_link_libraries(${lvgl_lib} PUBLIC ${fatfs_lib})
