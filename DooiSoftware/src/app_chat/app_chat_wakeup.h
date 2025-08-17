/*
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int app_chat_wakeup_init(void);
int app_chat_wakeup_start(void);
int app_chat_wakeup_exit(void);
void app_chat_button_wakeup_enable(bool en);
#ifdef __cplusplus
}
#endif