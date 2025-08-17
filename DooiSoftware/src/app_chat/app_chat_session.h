/*
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include "lsc_session_voice.h"

#ifdef __cplusplus
extern "C" {
#endif

int app_chat_session_init(void);
int app_chat_session_set_interactive_mode(session_voice_interactive_mode_e mode);
int app_chat_session_vad_enable(bool en);
void app_chat_session_start(void);
bool if_app_chat_session_is_continue(void);
int app_chat_session_clear_memory(void);
int app_chat_session_clear_history(void);
int app_chat_revert_precision_recognition_mode(void);
void app_chat_session_send_audio(uint8_t *data, uint32_t len);
#ifdef __cplusplus
}
#endif