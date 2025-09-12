/*
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma GCC push_options
#pragma GCC optimize("O0")

__attribute__((section(".venus_cp.text"))) static const unsigned char venus_cp[] = {
#include "venus_cp.inc"
};

__attribute__((section(".littlefs.text"))) static const unsigned char littlefs[] = {
#include "littlefs.inc"
};

#if defined(CONFIG_CAPABILITY_SPD)
__attribute__((section(".spd_body_detect.text"))) static const unsigned char spd_body_detect[] = {
#include "spd_body_detect.inc"
};

__attribute__((section(".spd_key_points.text"))) static const unsigned char spd_key_points[] = {
#include "spd_key_points.inc"
};

__attribute__((section(".spd_sit_pose.text"))) static const unsigned char spd_sit_pose[] = {
#include "spd_sit_pose.inc"
};
#endif

#if defined(CONFIG_CAPABILITY_WAKEUP)
__attribute__((section(".wakeup_cae_mlp.text"))) static const unsigned char wakeup_cae_mlp[] = {
#include "wakeup_cae_mlp.inc"
};

__attribute__((section(".wakeup_ai_wrap_config.text"))) static const unsigned char wakeup_ai_wrap_config[] = {
#include "wakeup_ai_wrap_config.inc"
};

#endif

#if defined(CONFIG_CAPABILITY_QRCODE)
__attribute__((section(".qrcode_detect_res_thinker.text"))) static const unsigned char qrcode_detect[] = {
#include "qrcode_detect_res_thinker.inc"
};
#endif

#pragma GCC pop_options
