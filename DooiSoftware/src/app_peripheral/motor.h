#ifndef MOTOR_H
#define MOTOR_H

#include <zephyr/kernel.h>
#include <stdint.h>
#include <stdbool.h>

/* 电机速度定义 */
#define MOTOR_SPEED_MAX     100
#define MOTOR_SPEED_80      80
#define MOTOR_SPEED_60      60
#define MOTOR_SPEED_30      30
#define MOTOR_SPEED_MIN     0

/* PWM通道定义 */
#define MOTOR_LEFT_CHANNEL_A    12
#define MOTOR_LEFT_CHANNEL_B    13
#define MOTOR_RIGHT_CHANNEL_A   14
#define MOTOR_RIGHT_CHANNEL_B   15

/* 运动控制参数 */
#define MOTOR_ACCEL_STEPS       50      // 加速曲线点数
#define MOTOR_UPDATE_INTERVAL   20      // 20ms控制周期
#define MOTOR_DANCE_STACK_DEPTH 8       // 动作序列嵌套层数

/* 电机状态枚举 */
typedef enum {
    MOTOR_STATE_IDLE,           // 空闲状态
    MOTOR_STATE_MOVING,         // 平滑移动中
    MOTOR_STATE_STOPPING,       // 自动停止中
    MOTOR_STATE_ESTOP           // 紧急停止
} motor_state_t;

/* 动作步骤定义 */
typedef struct {
    int16_t left_speed;         // 左电机速度(-100~100)
    int16_t right_speed;        // 右电机速度(-100~100)
    uint16_t duration_ms;       // 步骤持续时间(ms)
} motor_step_t;

/* 电机控制结构体 */
typedef struct {
    motor_state_t state;        // 当前状态
    uint32_t last_update;       // 最后更新时间
    uint32_t step_start_time;   // 当前步骤开始时间
    int16_t current_left;       // 当前左电机速度
    int16_t current_right;      // 当前右电机速度
    int16_t target_left;        // 目标左电机速度
    int16_t target_right;       // 目标右电机速度
    uint32_t accel_table[MOTOR_ACCEL_STEPS]; // 加速曲线表
    uint16_t step_index;         // 当前步骤索引
    uint16_t step_total;         // 总步骤数
    bool auto_stop;             // 完成后自动停止标志
    uint32_t stop_timer;        // 自动停止计时器
} motor_ctrl_t;

/**
 * @brief 初始化电机控制模块
 */
void motor_init(void);

/**
 * @brief 唤醒电机控制器
 */
void motor_wakeup(void);

/**
 * @brief 使电机控制器进入睡眠状态
 */
void motor_sleep(void);

/**
 * @brief 平滑移动到指定速度
 * @param left 左电机目标速度(-100~100)
 * @param right 右电机目标速度(-100~100)
 * @param duration_ms 过渡时间(毫秒)
 */
void motor_move_smooth(int16_t left, int16_t right, uint16_t duration_ms);

/**
 * @brief 增强版平滑移动
 * @param left 左电机目标速度(-100~100)
 * @param right 右电机目标速度(-100~100)
 * @param duration_ms 过渡时间(毫秒)，0表示立即设置
 * @param auto_stop 完成后是否自动停止
 */
void motor_move_smooth_ex(int16_t left, int16_t right, uint16_t duration_ms, bool auto_stop);

/**
 * @brief 基于输入向量的运动控制
 * @param x 水平方向控制量(-1.0~1.0)
 * @param y 垂直方向控制量(-1.0~1.0)
 * @param duration_ms 平滑过渡时间(ms)，0表示立即设置
 */
void motor_motion_control(float x, float y, uint16_t duration_ms);

/**
 * @brief 执行预设动作序列
 * @param sequence 动作序列数组
 * @param steps 动作步骤数
 */
void motor_execute_dance(const motor_step_t *sequence, uint16_t steps);

/**
 * @brief 紧急停止所有电机
 */
void motor_emergency_stop(void);

/**
 * @brief 从紧急停止状态恢复
 */
void motor_resume(void);

/**
 * @brief 电机处理函数，需在主循环中定期调用
 */
void motor_process(void);

#endif // MOTOR_H
