file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_LIST_DIR}/*.cpp)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
idf_component_register(SRCS ${SOURCES}
                    PRIV_REQUIRES spi_flash lwip esp_netif esp_wifi nvs_flash jsmn esp_http_client esp-tls
                    INCLUDE_DIRS "system/include" "apps/include" "mylib/include")
                    