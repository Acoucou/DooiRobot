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
#include "servo.h"


/* PWM通道定义 */
static const struct pwm_dt_spec servos[] = {
    PWM_DT_SPEC_GET(DT_ALIAS(pwm1_servo)),  // 舵机1
    PWM_DT_SPEC_GET(DT_ALIAS(pwm3_servo)),  // 舵机2
};

#define GPIOA_3_NODE_D DT_ALIAS(motor_ctrl)
static const struct gpio_dt_spec gpio_exd3 = GPIO_DT_SPEC_GET(GPIOA_3_NODE_D, gpios);

#define GPIOD_6_NODE_D DT_ALIAS(motor_ctrl2)
static const struct gpio_dt_spec gpio_exd6 = GPIO_DT_SPEC_GET(GPIOD_6_NODE_D, gpios);

#define PWM_OFFSET  50000

int servo1_offset = 50000;
int servo2_offset = 50000;

/* 舵机参数 */
#define PWM_PERIOD_NS     20000000   // 20ms周期(50Hz)
#define MIN_PULSE_NS       500000    // 0.5ms(0度)
#define MAX_PULSE_NS      2500000    // 2.5ms(180度)
#define ANGLE_RANGE          180     // 角度范围

void servo_enable() {
    gpio_pin_configure_dt(&gpio_exd3, GPIO_OUTPUT_LOW);
    gpio_pin_configure_dt(&gpio_exd6, GPIO_OUTPUT_LOW);
}

void servo_disable() {
    gpio_pin_configure_dt(&gpio_exd3, GPIO_OUTPUT_HIGH);
    gpio_pin_configure_dt(&gpio_exd6, GPIO_OUTPUT_HIGH);
}


/* 舵机动作控制结构 */
typedef struct {
    uint8_t speed;               // 动作速度(1-10)
    uint8_t tick;
    bool update_needed;          // 需要更新标志
} servo_control_t;

static servo_control_t m_servo;

/* 将角度转换为PWM脉冲宽度 */
static uint32_t angle_to_pulse(int angle)
{
    angle = CLAMP(angle, 0, ANGLE_RANGE);   /* 限制角度范围 */

    return MIN_PULSE_NS + (angle * (MAX_PULSE_NS - MIN_PULSE_NS)) / ANGLE_RANGE;  /* 线性映射角度到脉冲宽度 */
}

/* 立即更新舵机角度 */
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
    
    /* 初始化控制结构 */
    memset(&m_servo, 0, sizeof(m_servo));
    
    /* 初始位置设为90度 */
    servo_set_angle(SERVO_BOTH, 90);
}

void servo_set_angle(servo_select_t servo, int angle)
{
    m_servo.update_needed = true;
    m_servo.tick = 0;
    servo_enable();
    
    apply_angle(servo, angle);    /* 立即设置角度 */
}

void servo_process(void)
{
    if(m_servo.update_needed)
    {
        if(m_servo.tick++ > 25)
        {
            m_servo.tick = 0;
            servo_disable();
            m_servo.update_needed = false;
        }
    }
}