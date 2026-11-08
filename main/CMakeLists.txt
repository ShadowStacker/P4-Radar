file(GLOB_RECURSE APP_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/core/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/third_party/tjpgd/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/app/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/ui/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/data/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/platform/esp/*.cpp")

# The "What's new" notes come from the changelog.
include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake_changelog.cmake")
plane_radar_changelog("${CMAKE_CURRENT_SOURCE_DIR}/../CHANGELOG.md" "${CMAKE_CURRENT_BINARY_DIR}/changelog_data.cpp")

idf_component_register(
  SRCS "main.cpp" ${APP_SOURCES} "${CMAKE_CURRENT_BINARY_DIR}/changelog_data.cpp"
  INCLUDE_DIRS "."
  REQUIRES nvs_flash esp_netif esp_event esp_http_client esp_timer esp_app_format esp_lcd esp_mm fatfs sdmmc esp_driver_sdmmc json mbedtls esp_https_ota app_update esp_http_server mqtt)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-missing-field-initializers)
# The display panel is created through our wrapper (see platform/esp/esp_display.cpp).
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_lcd_new_panel_dpi")
# LVGL's memory functions live in platform/esp/esp_lv_mem.cpp; only LVGL calls
# them, so tell the linker to keep that file.
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--undefined=lv_malloc_core")
