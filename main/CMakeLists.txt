file(GLOB_RECURSE APP_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/core/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/third_party/tjpgd/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/app/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/ui/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/data/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/platform/esp/*.cpp")

idf_component_register(
  SRCS "main.cpp" ${APP_SOURCES}
  INCLUDE_DIRS "."
  REQUIRES nvs_flash esp_netif esp_event esp_http_client esp_timer esp_app_format esp_lcd esp_mm fatfs sdmmc esp_driver_sdmmc json mbedtls)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-missing-field-initializers)
