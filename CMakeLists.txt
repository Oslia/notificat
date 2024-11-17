# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(notificat CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
idf_component_get_property(freetype_lib espressif__freetype COMPONENT_LIB)
target_link_libraries(${lvgl_lib} PUBLIC ${freetype_lib})

idf_component_get_property(fatfs_lib fatfs COMPONENT_LIB)
target_link_libraries(${lvgl_lib} PUBLIC ${fatfs_lib})