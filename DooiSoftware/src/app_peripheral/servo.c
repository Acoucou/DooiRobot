/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(servo, LOG_LEVEL_INF);
#include <string.h>
#include <stdlib.h>
#include "servo.h"

/* PWM通道定义 */
static const struct pwm_dt_spec servos[] = {
    PWM_DT_SPEC_GET(DT_ALIAS(pwm1_servo)),  // 舵机1（左）
    PWM_DT_SPEC_GET(DT_ALIAS(pwm3_servo)),  // 舵机2（右）
};

#define GPIOA_3_NODE_D DT_ALIAS(motor_ctrl)
static const struct gpio_dt_spec gpio_exd3 = GPIO_DT_SPEC_GET(GPIOA_3_NODE_D, gpios);

#define GPIOD_6_NODE_D DT_ALIAS(motor_ctrl2)
static const struct gpio_dt_spec gpio_exd6 = GPIO_DT_SPEC_GET(GPIOD_6_NODE_D, gpios);

/* 可选的脉宽偏移校准 */
#define PWM_OFFSET  50000
int servo1_offset = 50000;
int servo2_offset = 50000;

/* 舵机参数 */
#define PWM_PERIOD_NS     20000000   // 20ms周期(50Hz)
#define MIN_PULSE_NS       500000    // 0.5ms(0度)
#define MAX_PULSE_NS      2500000    // 2.5ms(180度)
#define ANGLE_RANGE          180     // 角度范围

/* 运动学参数（按实际舵机调整）-----------------------------------------
 * 典型小舵机(如SG90)约 0.12~0.20s / 60°。
 * 下面给出一个保守默认：0.20s/60° => 3.333ms/°
 * 建议实机标定：把 SERVO_60DEG_MS 改成你的舵机真实值。
 */
#ifndef SERVO_60DEG_MS
#define SERVO_60DEG_MS 200u  // 200ms 走 60°
#endif
#define SERVO_MS_PER_DEG ((float)SERVO_60DEG_MS / 60.0f)
/* 运动完成后的静止余量，避免临界点提前断电 */
#ifndef SERVO_SETTLE_MS
#define SERVO_SETTLE_MS 30u
#endif
/* -------------------------------------------------------------------- */

/* 舵机动作控制结构（基于角度位移的时长估算） */
typedef struct {
    /* 运动学估算 */
    uint32_t start_ms;          // 本次运动起始时间（ms）
    uint32_t need_ms;           // 预计需要的运动时长（ms）——两舵机取最大
    uint16_t settle_ms;         // 额外静止余量（ms）

    /* 角度状态（逻辑角度，0~180） */
    int current_angle[2];       // 记录逻辑“当前角度”（软件侧）
    int target_angle[2];        // 记录本次目标角度

    bool moving;                // 正在运动标志
    bool update_needed;         // 需要更新标志（供调试，可不必）
} servo_control_t;

static servo_control_t m_servo;


void servo_enable(void) {
    gpio_pin_configure_dt(&gpio_exd3, GPIO_OUTPUT_LOW);
    gpio_pin_configure_dt(&gpio_exd6, GPIO_OUTPUT_LOW);
}

void servo_disable(void) {
    gpio_pin_configure_dt(&gpio_exd3, GPIO_OUTPUT_HIGH);
    gpio_pin_configure_dt(&gpio_exd6, GPIO_OUTPUT_HIGH);
}

/* 将角度转换为PWM脉冲宽度 */
static uint32_t angle_to_pulse(int angle)
{
    angle = CLAMP(angle, 0, ANGLE_RANGE);   /* 限制角度范围 */
    return MIN_PULSE_NS + (angle * (MAX_PULSE_NS - MIN_PULSE_NS)) / ANGLE_RANGE;
}

/* 立即更新舵机角度到目标 */
static void apply_angle(servo_select_t servo, int angle)
{
    uint32_t pulse_ns = 0;

    switch (servo) {
    case SERVO_LEFT:
        pulse_ns = angle_to_pulse(angle) + servo1_offset;
        pwm_set_pulse_dt(&servos[0], pulse_ns);
        break;

    case SERVO_RIGHT:
        pulse_ns = angle_to_pulse(180 - angle) + servo2_offset;
        pwm_set_pulse_dt(&servos[1], pulse_ns);
        break;

    case SERVO_BOTH:
        pulse_ns = angle_to_pulse(angle) + servo1_offset;
        pwm_set_pulse_dt(&servos[0], pulse_ns);
        pulse_ns = angle_to_pulse(180 - angle) + servo2_offset;
        pwm_set_pulse_dt(&servos[1], pulse_ns);
        break;
    }
}

/* 估算两角度间需要的时长（ms） */
static inline uint32_t estimate_ms_for_delta(int from_deg, int to_deg)
{
    int delta = abs(to_deg - from_deg);
    float ms = (float)delta * SERVO_MS_PER_DEG;
    /* 向上取整，保证不断电过早 */
    uint32_t ms_i = (uint32_t)ms;
    if ((float)ms_i < ms) { ms_i++; }
    return ms_i;
}

/* 统一计算一次动作需要的最长时间 */
static uint32_t compute_motion_time_ms(void)
{
    uint32_t ms_left  = estimate_ms_for_delta(m_servo.current_angle[0], m_servo.target_angle[0]);
    uint32_t ms_right = estimate_ms_for_delta(m_servo.current_angle[1], m_servo.target_angle[1]);
    uint32_t ms_need  = (ms_left > ms_right) ? ms_left : ms_right;

    /* 给点余量，叠加 settle */
    ms_need += m_servo.settle_ms;
    return ms_need;
}

void servo_init(void)
{
    /* 检查PWM设备是否就绪 */
    for (int i = 0; i < ARRAY_SIZE(servos); i++) {
        if (!device_is_ready(servos[i].dev)) {
            LOG_ERR("PWM device %d not ready", i);
            return;
        }
    }

    servo_enable();

    memset(&m_servo, 0, sizeof(m_servo));
    m_servo.settle_ms = SERVO_SETTLE_MS;

    /* 初始角度假定 90°（软件状态与实际输出对齐） */
    m_servo.current_angle[0] = 0;
    m_servo.current_angle[1] = 0;
    m_servo.target_angle[0]  = 90;
    m_servo.target_angle[1]  = 90;
    m_servo.moving = false;

    servo_set_angle(SERVO_BOTH, 90);
}

void servo_set_angle(servo_select_t servo, int angle)
{
    angle = CLAMP(angle, 0, ANGLE_RANGE);

    /* 更新目标角度 */
    switch (servo) {
    case SERVO_LEFT:
        m_servo.target_angle[0] = angle;
        break;
    case SERVO_RIGHT:
        m_servo.target_angle[1] = angle;
        break;
    case SERVO_BOTH:
        m_servo.target_angle[0] = angle;
        m_servo.target_angle[1] = angle;
        break;
    }

    /* 立即输出 PWM 到目标（让舵机自行跑过去） */
    servo_enable();
    apply_angle(servo, angle);

    /* 重新估算动作时长（两舵机取最大）并启动计时 */
    m_servo.need_ms  = compute_motion_time_ms();
    m_servo.start_ms = k_uptime_get_32();
    m_servo.moving   = true;
    m_servo.update_needed = true;

    // LOG_DBG("servo_set_angle: L %d->%d, R %d->%d, need=%ums",
    //         m_servo.current_angle[0], m_servo.target_angle[0],
    //         m_servo.current_angle[1], m_servo.target_angle[1],
    //         m_servo.need_ms);
}

void servo_process(void)
{
    if (!m_servo.moving) {
        return;
    }

    uint32_t now = k_uptime_get_32();
    uint32_t elapsed = now - m_servo.start_ms;

    if (elapsed >= m_servo.need_ms) {
        /* 认为已到位：更新软件侧当前角度、断电省能 */
        m_servo.current_angle[0] = m_servo.target_angle[0];
        m_servo.current_angle[1] = m_servo.target_angle[1];

        servo_disable();
        m_servo.moving = false;
        m_servo.update_needed = false;
    }
}