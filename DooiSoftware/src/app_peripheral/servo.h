#ifndef __SERVO_H
#define __SERVO_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>

/* 舵机选择枚举 */
typedef enum {
    SERVO_LEFT,     // 左舵机
    SERVO_RIGHT,    // 右舵机
    SERVO_BOTH      // 同时控制两个舵机
} servo_select_t;

/* 预设动作类型 */
typedef enum {
    SERVO_ACTION_NONE,              // 无动作
    SERVO_ACTION_WAVE,              // 挥手
    SERVO_ACTION_NOD,               // 点头
    SERVO_ACTION_REACHING,          // 伸手
    SERVO_ACTION_SHRUG,             // 耸肩
    SERVO_ACTION_CLAPPING,          // 拍手
    SERVO_ACTION_HANDS_ON_HIPS,     // 双手叉腰
    SERVO_ACTION_SIDE_SWINGING      // 手舞足蹈
} servo_action_t;

/* 舵机状态 */
typedef enum {
    SERVO_STATE_IDLE,       // 空闲状态
    SERVO_STATE_MOVING,     // 运动中
    SERVO_STATE_PAUSED      // 暂停状态
} servo_state_t;

/**
 * @brief 初始化舵机模块
 */
void servo_init(void);

/**
 * @brief 使能舵机电源
 */
void servo_enable(void);

/**
 * @brief 禁用舵机电源
 */
void servo_disable(void);



/**
 * @brief 设置舵机角度
 * @param servo 选择舵机(SERVO_LEFT/SERVO_RIGHT/SERVO_BOTH)
 * @param angle 目标角度(0-180)
 */
void servo_set_angle(servo_select_t servo, int angle);

/**
 * @brief 执行预设动作
 * @param action 动作类型
 * @param speed 动作速度(1-10)
 */
void servo_start_action(servo_action_t action, uint8_t speed);

/**
 * @brief 舵机处理函数，需在主循环中定期调用
 */
void servo_process(void);

#endif /* __SERVO_H */
