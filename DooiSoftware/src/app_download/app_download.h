/*
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int app_download(const char *url, uint8_t **out_data, int *out_len);
int app_download_free(void *ptr);

#ifdef __cplusplus
}
#endif
