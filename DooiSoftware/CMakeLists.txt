# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

set(SDK_ROOT_PATH   ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
set(BOARD_ROOT      ${SDK_ROOT_PATH})
set(SNIPPET_ROOT    ${SDK_ROOT_PATH}/boards)

list(APPEND ZEPHYR_EXTRA_MODULES ${SDK_ROOT_PATH}/components)
list(APPEND ZEPHYR_EXTRA_MODULES ${SDK_ROOT_PATH}/components/audioplayer)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(llm_control)

target_sources(app PRIVATE 
        src/main.c 
        # src/aiui_nlp_consumer.c
        )

target_link_libraries(app PRIVATE ls_chat lisa)

add_subdirectory(src/app_ui/)
add_subdirectory(src/app_connect)
add_subdirectory(src/app_obj_recognition)
add_subdirectory(src/app_camera)
add_subdirectory(src/app_chat)
add_subdirectory(src/app_wifi_qrcode_scan)
add_subdirectory(src/app_dev_qrcode_scan)
add_subdirectory(src/app_download)
add_subdirectory(src/app_http_server)
add_subdirectory(src/app_peripheral)
add_subdirectory(src/app_ble_connect)
add_subdirectory(lib/ico)

zephyr_include_directories(src/)

if(CONFIG_APPLICATION_PACK_IMAGES)
    add_subdirectory(image)
endif()

zephyr_library_sources_ifdef(CONFIG_GCL_COMP_RES       resource/resource.c)
zephyr_include_directories_ifdef(CONFIG_GCL_COMP_RES   resource)
# target_include_directories(app PRIVATE src)
zephyr_library_link_libraries(lsf_client)
