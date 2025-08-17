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

typedef enum {
	APP_CHAT_EVT_GOT_VAD,
	APP_CHAT_EVT_TTS_PLAY,
	APP_CHAT_EVT_IAT,
	APP_CHAT_EVT_REPLY_TXT,
	APP_CHAT_EVT_MUSIC_LISTS,
	APP_CHAT_EVT_MUSIC_INSTR,
	APP_CHAT_EVT_AIUI_CTRL,
	APP_CHAT_EVT_DRAW,
	APP_CHAT_EVT_FINISH,
	APP_CHAT_EVT_ERR,
	APP_CHAT_EVT_WAKEUP_ASR,
	APP_CHAT_EVT_WAKEUP_BUTTON,
	APP_CHAT_EVT_NONE,
} app_chat_evt_e;

typedef struct {
	app_chat_evt_e event;
	void *data;
	uint32_t len;
} app_chat_evt_msg_t;

int app_chat_evt_notify(app_chat_evt_e evt, void *data, uint32_t len);
int app_chat_init(void);
int app_chat_stop(void);
int app_chat_start(void);
int app_chat_exit(void);

#ifdef __cplusplus
}
#endif