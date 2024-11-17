file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

idf_component_register(SRCS ${SOURCES}
                    PRIV_REQUIRES spi_flash lwip esp_netif esp_wifi nvs_flash
                    INCLUDE_DIRS "system/include" "apps/include")
