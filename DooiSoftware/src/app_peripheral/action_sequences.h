#ifndef ACTION_SEQUENCES_H_
#define ACTION_SEQUENCES_H_

#include "app_peripherals.h"

// 动作类型定义
typedef enum {
    ACTION_NONE = 0,
    ACTION_EYE_WINK,
    ACTION_EYE_MOVE,
    ACTION_LED_SET,
    ACTION_LED_BREATHE,
    ACTION_LED_BLINK,
    ACTION_LED_RAINBOW,
    ACTION_LED_OFF,
    ACTION_SERVO_MOVE,
    ACTION_MOTOR_CONTROL,
    ACTION_EXPRESSION,
    ACTION_DELAY
} action_type_t;
void led_set_blink(int color, uint16_t period_ms, bool symmetric);
// 动作指令结构体
typedef struct {
    action_type_t type;
    union {
        struct { uint16_t duration; } eye_wink;
        struct { float x, y; uint16_t duration; } eye_move;
        struct { uint32_t left_color, right_color; } led_set;
        struct { uint32_t color; uint16_t period; bool symmetric; } led_breathe;
        struct { int color; uint16_t period_ms; bool symmetric; } led_blink;
        struct { uint16_t period_ms; bool symmetric; } led_rainbow;
        struct { servo_select_t servo; uint8_t angle; } servo_move;
        struct { int16_t left_speed, right_speed; uint16_t duration; } motor_control;
        struct { uint16_t animation; } expression;
        struct { uint16_t delay_ms; } delay;
    } params;
} action_cmd_t;

// 序列类型枚举
typedef enum {
    // 基础表情
    SEQUENCE_SURPRISED,         // 惊讶
    SEQUENCE_ANGRY,             // 生气
    SEQUENCE_ANGRY1,
    SEQUENCE_ANGRY_FOCUS_LEFT, 
    SEQUENCE_ANGRY_FOCUS_RIGHT,
    SEQUENCE_HAPPY,             // 开心  
    SEQUENCE_HAPPY1,
    SEQUENCE_HAPPY2,
    SEQUENCE_SAD,               // 悲伤

    // 肢体动作
    SEQUENCE_FOCUS_BELOW_CENTER,
    SEQUENCE_FOCUS_TOP_CENTER,
    SEQUENCE_FOCUS_TOP_LEFT,
    SEQUENCE_FOCUS_TOP_RIGHT,
    SEQUENCE_FOCUS_LEFT,
    SEQUENCE_FOCUS_RIGHT,
    SEQUENCE_DANCE_ROUTINE,     // 跳舞
    SEQUENCE_GREETING,          // 问候
    SEQUENCE_WAVE_GOODBYE,      // 挥手告别
    SEQUENCE_HUG,               // 拥抱
    SEQUENCE_NOD_AGREE,         // 点头同意
    SEQUENCE_SHAKE_NO,          // 摇头拒绝
    
    // 认知状态
    SEQUENCE_THINKING,          // 思考
    SEQUENCE_CONFUSED,          // 困惑
    SEQUENCE_PATROL,            // 巡逻
    SEQUENCE_SEARCH,            // 寻找
    
    // 生理状态
    SEQUENCE_SLEEP,             // 睡觉
    SEQUENCE_CHARGING,          // 充电 
    SEQUENCE_LOW_BATTERY,       // 低电量 
    
    // 特殊互动
    SEQUENCE_COMBO_ACTION,      // 矫情
    SEQUENCE_CRY,               // 哭泣
    SEQUENCE_DISDAIN,           // 鄙视
    SEQUENCE_FEAR_EXTENDED,     // 害怕
    SEQUENCE_SHY,               // 害羞 

    // 页面默认动作
    SEQUENCE_EYES_DEFAULT,          // 眼睛页默认
    SEQUENCE_EYES_FIRST_WAKEUP,     // 眼睛页首次唤醒
    SEQUENCE_MUSIC_DEFAULT,         // 音乐页默认
    SEQUENCE_LUCKY_CAT_DEFAULT,     // 招财猫页默认
    SEQUENCE_CAMERA_DEFAULT,        // 摄像头页默认
    SEQUENCE_CLOCK_DEFAULT,         // 时钟页默认
    SEQUENCE_GENERAL_DEFAULT,       // 通用默认

    SEQUENCE_COUNT
} sequence_type_t;

// 动作序列结构体
typedef struct {
    const action_cmd_t *actions; // 指向动作数组的指针（常量，存放在Flash）
    uint8_t count;               // 动作数量
} action_sequence_t;

// 重置动作队列
void action_sequences_reset(void);

// 添加单个动作到队列
bool action_sequences_enqueue_action(action_cmd_t cmd);

// 添加预定义序列到队列
bool action_sequences_enqueue(sequence_type_t seq_type);

// 添加带参数的动作序列（用于动态序列）
bool action_sequences_enqueue_custom(const action_cmd_t *actions, uint8_t count);

// 获取当前队列状态
bool action_sequences_is_queue_empty(void);

// 获取当前执行的动作
action_type_t action_sequences_get_current_action(void);

// 动作处理函数
void action_sequences_process(void);

#endif /* ACTION_SEQUENCES_H_ */
