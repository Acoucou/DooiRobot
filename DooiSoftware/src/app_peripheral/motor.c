#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <stdint.h>
#include <math.h>
#include "motor.h"
#include "pca9635.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(motor, LOG_LEVEL_INF);

static motor_ctrl_t m_motor;

#define GPIOA_2_NODE DT_ALIAS(gpioa2)
static const struct gpio_dt_spec gpioa2 = GPIO_DT_SPEC_GET(GPIOA_2_NODE, gpios);

void motor_wakeup()
{
    gpio_pin_set_dt(&gpioa2, 1);
}

void motor_sleep()
{
    gpio_pin_set_dt(&gpioa2, 0);
}

/* 生成S曲线加速表 */
static void generate_accel_table(void) {
    const float factor = 2 * 3.14159f / MOTOR_ACCEL_STEPS;
    for (int i = 0; i < MOTOR_ACCEL_STEPS; i++) {
        float ratio = (1 - cosf(i * factor)) / 2;
        m_motor.accel_table[i] = (uint32_t)(ratio * 1000);
    }
}

/* 驱动电机硬件 */
static void drive_motors(int16_t left, int16_t right) {
    uint8_t buff[2] = {0};

    /* 左电机控制 */
    if (left >= 0) {
        // pca9635_set_pwm(MOTOR_LEFT_CHANNEL_A, (left * 255) / 100);
        // pca9635_set_pwm(MOTOR_LEFT_CHANNEL_B, 0);

        buff[0] = (left * 255) / 100;
        buff[1] = 0;

        pca9635_set_pwms(0xAC, buff, 2);
    } else {
        // pca9635_set_pwm(MOTOR_LEFT_CHANNEL_A, 0);
        // pca9635_set_pwm(MOTOR_LEFT_CHANNEL_B,
        buff[0] = 0;
        buff[1] = (-left * 255) / 100;

        pca9635_set_pwms(0xAC, buff, 2);
    }

    /* 右电机控制 */
    if (right >= 0) {
        // pca9635_set_pwm(MOTOR_RIGHT_CHANNEL_A, (right * 255) / 100);
        // pca9635_set_pwm(MOTOR_RIGHT_CHANNEL_B, 0);
        buff[0] = (right * 255) / 100;
        buff[1] = 0;

        pca9635_set_pwms(0xAE, buff, 2);
    } else {
        // pca9635_set_pwm(MOTOR_RIGHT_CHANNEL_A, 0);
        // pca9635_set_pwm(MOTOR_RIGHT_CHANNEL_B, (-right * 255) / 100);
        buff[0] = 0;
        buff[1] = (-right * 255) / 100;

        pca9635_set_pwms(0xAE, buff, 2);
    }

    LOG_DBG("Motors set to L:%d R:%d", left, right);
}

/* 计算当前速度(带S曲线加速) */
static void calculate_speed(void) {
    uint32_t now = k_uptime_get();
    
    // 检查是否到达更新时间
    if (now - m_motor.last_update < MOTOR_UPDATE_INTERVAL) {
        return;
    }
    m_motor.last_update = now;

    // 确保有足够的步骤
    if (m_motor.step_total == 0) {
        LOG_ERR("Invalid step_total: 0");
        m_motor.state = MOTOR_STATE_IDLE;
        return;
    }

    /* 修复1: 检测方向变化并重置加速曲线 */
    bool direction_changed = 
        (m_motor.target_left * m_motor.current_left < 0) || 
        (m_motor.target_right * m_motor.current_right < 0);
    
    if (direction_changed) {
        // 方向改变时重置加速曲线
        m_motor.current_left = 0;
        m_motor.current_right = 0;
        m_motor.step_index = 0;
        LOG_DBG("Direction change detected, resetting acceleration");
    }

    /* 计算加速比例 */
    uint32_t progress = (m_motor.step_index * 1000) / m_motor.step_total;
    uint32_t accel_step = (progress * MOTOR_ACCEL_STEPS) / 1000;
    accel_step = MIN(accel_step, MOTOR_ACCEL_STEPS - 1);
    float accel_ratio = m_motor.accel_table[accel_step] / 1000.0f;

    /* 应用加速曲线 */
    int16_t delta_left = m_motor.target_left - m_motor.current_left;
    int16_t delta_right = m_motor.target_right - m_motor.current_right;
    
    /* 修复2: 方向改变时使用更快的加速曲线 */
    if (direction_changed) {
        // 方向改变时使用线性加速，跳过S曲线
        float linear_ratio = (float)(m_motor.step_index + 1) / m_motor.step_total;
        m_motor.current_left = m_motor.current_left + (int16_t)(delta_left * linear_ratio);
        m_motor.current_right = m_motor.current_right + (int16_t)(delta_right * linear_ratio);
    } else {
        // 正常情况使用S曲线加速
        m_motor.current_left += (int16_t)(delta_left * accel_ratio);
        m_motor.current_right += (int16_t)(delta_right * accel_ratio);
    }

    /* 边界检查 */
    m_motor.current_left = CLAMP(m_motor.current_left, -MOTOR_SPEED_MAX, MOTOR_SPEED_MAX);
    m_motor.current_right = CLAMP(m_motor.current_right, -MOTOR_SPEED_MAX, MOTOR_SPEED_MAX);

    /* 驱动电机 */
    drive_motors(m_motor.current_left, m_motor.current_right);

    /* 检查是否完成 */
    if (++m_motor.step_index >= m_motor.step_total) {
        if (m_motor.auto_stop) {
            m_motor.stop_timer = now;
            m_motor.state = MOTOR_STATE_STOPPING;
        } else {
            m_motor.state = MOTOR_STATE_IDLE;
        }
        motor_sleep();
        LOG_INF("Movement completed");
    }
}

/* 新增停止处理 */
static void handle_auto_stop(void) {
    uint32_t now = k_uptime_get();
    
    // 确保有足够的停止时间
    if (now - m_motor.stop_timer >= MOTOR_UPDATE_INTERVAL) {
        m_motor.current_left = 0;
        m_motor.current_right = 0;
        drive_motors(0, 0);
        m_motor.state = MOTOR_STATE_IDLE;
        LOG_INF("Auto stop completed");
    }
}

void motor_init(void) {
    // 初始化GPIO
    if (!device_is_ready(gpioa2.port)) {
        LOG_ERR("GPIO device not ready");
        return;
    }
    gpio_pin_configure_dt(&gpioa2, GPIO_OUTPUT_INACTIVE);

    // 初始化控制结构
    memset(&m_motor, 0, sizeof(m_motor));
    m_motor.state = MOTOR_STATE_IDLE;
    m_motor.last_update = k_uptime_get();
    
    // 生成加速曲线
    generate_accel_table();
    
    LOG_INF("Motor controller initialized");
}

void motor_move_smooth(int16_t left, int16_t right, uint16_t duration_ms) {
    motor_move_smooth_ex(left, right, duration_ms, true);
}

/* 新增函数实现 */
void motor_move_smooth_ex(int16_t left, int16_t right, uint16_t duration_ms, bool auto_stop) 
{
    // if (m_motor.state == MOTOR_STATE_ESTOP) return;
    motor_wakeup();

    // 设置目标参数
    m_motor.target_left = CLAMP(left, -MOTOR_SPEED_MAX, MOTOR_SPEED_MAX);
    m_motor.target_right = CLAMP(right, -MOTOR_SPEED_MAX, MOTOR_SPEED_MAX);
    m_motor.step_total = MAX(duration_ms / MOTOR_UPDATE_INTERVAL, 1);
    m_motor.step_index = 0;
    m_motor.state = MOTOR_STATE_MOVING;
    m_motor.last_update = k_uptime_get();
    m_motor.auto_stop = auto_stop;
    
    if (duration_ms == 0) {
        // 立即设置
        m_motor.current_left = m_motor.target_left;
        m_motor.current_right = m_motor.target_right;
        drive_motors(m_motor.current_left, m_motor.current_right);
        m_motor.state = MOTOR_STATE_IDLE;
    }
    
    LOG_INF("Move smooth to L:%d R:%d in %dms, auto_stop:%d", 
           left, right, duration_ms, auto_stop);
}

void motor_motion_control(float x, float y, uint16_t duration_ms) 
{
    /* 参数范围检查 */
    x = CLAMP(x, -1.0f, 1.0f);
    y = CLAMP(y, -1.0f, 1.0f);
    
    /* 计算基础速度和转向调整 */
    float base_speed = y * MOTOR_SPEED_MAX;
    float turn_adjust = x * MOTOR_SPEED_MAX;

    /* 计算左右电机速度 */
    int left_speed = (int)(base_speed + turn_adjust);
    int right_speed = (int)(base_speed - turn_adjust);
    
    /* 速度限幅 */
    left_speed = CLAMP(left_speed, -MOTOR_SPEED_MAX, MOTOR_SPEED_MAX);
    right_speed = CLAMP(right_speed, -MOTOR_SPEED_MAX, MOTOR_SPEED_MAX);

    LOG_DBG("Motion control: X=%.2f Y=%.2f -> L:%d R:%d", x, y, left_speed, right_speed);
    
    /* 设置电机速度 */
    motor_move_smooth(left_speed, right_speed, duration_ms);
}



void motor_emergency_stop(void) {
    m_motor.state = MOTOR_STATE_ESTOP;
    drive_motors(0, 0);
    LOG_ERR("EMERGENCY STOP activated");
}

void motor_resume(void) {
    if (m_motor.state == MOTOR_STATE_ESTOP) {
        m_motor.state = MOTOR_STATE_IDLE;
        LOG_INF("Motor control resumed");
    }
}

void motor_process(void) {
    switch (m_motor.state) {
    case MOTOR_STATE_MOVING:
        calculate_speed();
        break;
        
    case MOTOR_STATE_STOPPING:
        handle_auto_stop();
        break;
        
    case MOTOR_STATE_ESTOP:
        break;
        
    default:
        break;
    }
}
