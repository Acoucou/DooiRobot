# SPDX-License-Identifier: Apache-2.0
set(gen_dir ${ZEPHYR_BINARY_DIR}/include/generated)
include(${CMAKE_CURRENT_LIST_DIR}/app_extensions.cmake)
add_custom_target(res_image_dependencies)

set(cp_firmware_file ${CMAKE_CURRENT_LIST_DIR}/../resource/cp_bin_files/venus_cp.bin)

if(EXISTS ${cp_firmware_file})
  app_generate_inc_file_for_target(
    app
    ${cp_firmware_file}
    ${gen_dir}/venus_cp.inc
    res_image_dependencies
  )
else()
  message(FATAL_ERROR "can not find the remote firmware file(${cp_firmware_file})")
endif()


app_generate_inc_file_for_target(
  app
  ../resource/littlefs/littlefs.bin
  ${gen_dir}/littlefs.inc
  res_image_dependencies
)

app_generate_inc_file_for_target(
  app
  ../resource/algo_resource/spd/body_detect_res_thinker.bin
  ${gen_dir}/spd_body_detect.inc
  res_image_dependencies
)

app_generate_inc_file_for_target(
  app
  ../resource/algo_resource/spd/keypoints_detect_res_thinker.bin
  ${gen_dir}/spd_key_points.inc
  res_image_dependencies
)

app_generate_inc_file_for_target(
  app
  ../resource/algo_resource/spd/sitpose_detect_res_thinker.bin
  ${gen_dir}/spd_sit_pose.inc
  res_image_dependencies
)

app_generate_inc_file_for_target(
  app
  ../resource/algo_resource/wakeup/cae_esr.bin
  ${gen_dir}/wakeup_cae_mlp.inc
  res_image_dependencies
)

app_generate_inc_file_for_target(
  app
  ../resource/algo_resource/wakeup/wrap.json
  ${gen_dir}/wakeup_ai_wrap_config.inc
  res_image_dependencies
)

app_generate_inc_file_for_target(
  app
  ../resource/algo_resource/qrcode/qrcode_detect_res_thinker.bin
  ${gen_dir}/qrcode_detect_res_thinker.inc
  res_image_dependencies
)


zephyr_linker_sources(SECTIONS custom-sections.ld)

target_sources(app PRIVATE ./dsp_resource.c)
