/*
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stddef.h>
#include "ls_cmd.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	APP_CONNECT_STATUS_DISCONNECTED = BIT(0),
	APP_CONNECT_STATUS_CONNECTED    = BIT(1),
	APP_CONNECT_STATUS_AUTH_FAILD   = BIT(2),
} app_connect_status_e;

typedef struct {
	char chip_id[DEVICE_CHIP_ID_LENGTH];
	char product_id[DEVICE_PRODUCT_ID_LENGTH];
	char secret_id[DEVICE_SECRET_ID_LENGTH];
} device_info_t;
int device_info_read(device_info_t *inf);

int app_connect_init(void);
int app_reconnect(void);
app_connect_status_e app_connect_get_status(void);

#ifdef __cplusplus
}
#endif