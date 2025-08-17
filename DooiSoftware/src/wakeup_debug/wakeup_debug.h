/*
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

void usb_cdc_acm_init(void);

void usb_send_data(uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif
