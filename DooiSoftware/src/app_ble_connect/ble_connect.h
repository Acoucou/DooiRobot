/*
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int ble_connect_task_init(void);
/**
 * @brief 初始化蓝牙功能
 *
 * @return int 0表示成功,负值表示错误码
 */
int ble_init(void);

/**
 * @brief 开始蓝牙广播
 *
 * @return int 0表示成功,负值表示错误码
 */
int ble_start_adv(void);

/**
 * @brief 停止蓝牙广播
 *
 * @return int 0表示成功,负值表示错误码
 */
int ble_stop_adv(void);

#ifdef __cplusplus
}
#endif