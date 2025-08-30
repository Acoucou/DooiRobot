#include "action_sequences.h"
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <stdlib.h>  // 用于 strtol
#include <stdio.h>   // 用于 snprintf
#include "eyes_control.h"
LOG_MODULE_REGISTER(action_sequences, LOG_LEVEL_DBG);

/* ------------------------- 预定义动作序列 ------------------------- */

// 惊讶序列（眼睛睁大，手臂向后，快速后退，蓝灯闪烁）
static const action_cmd_t surprised_sequence[] = {
    #if (USE_EYE_ENMOTION)
        {.type = ACTION_EXPRESSION, .params.expression = {EYES_EMOTION_AMAZED}},
    #else
        {.type = ACTION_EXPRESSION, .params.expression = {ANIMATION_FEAR}},  // 惊讶表情
    #endif
    {.type = ACTION_LED_SET, .params.led_set = {0x0000FF, 0x0000FF}},  // 蓝灯
    // {.type = ACTION_EYE_MOVE, .params.eye_move = {0.0f, 1.0f, 200}},     // 眼睛向上睁大
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_BOTH, 180}},  // 手臂向后举起
    {.type = ACTION_MOTOR_CONTROL, .params.motor_control = {-100, -100, 300}}, // 快速后退
    {.type = ACTION_DELAY, .params.delay = {300}},
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_BOTH, 90}}    // 手臂复位中立
};

// 生气序列（手臂交叉，红灯，后退，愤怒表情）
static const action_cmd_t angry_sequence[] = {
    #if (USE_EYE_ENMOTION)
         {.type = ACTION_EXPRESSION, .params.expression = {EYES_EMOTION_ANGRY}},
    #else
        {.type = ACTION_EXPRESSION, .params.expression = {ANIMATION_ANGRY}},  // GIF_ANGRY是愤怒表情
    #endif
    {.type = ACTION_LED_SET, .params.led_set = {0xFF0000, 0xFF0000}},  // 红灯
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_LEFT, 120}},  // 左手交叉
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_RIGHT, 60}},  // 右手交叉
    {.type = ACTION_DELAY, .params.delay = {500}},
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_LEFT, 60}},  
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_RIGHT, 120}},
    {.type = ACTION_DELAY, .params.delay = {500}},
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_LEFT, 120}},  // 左手交叉
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_RIGHT, 60}},  // 右手交叉
    {.type = ACTION_DELAY, .params.delay = {500}},
    // {.type = ACTION_EYE_MOVE, .params.eye_move = {0.0f, -0.5f, 300}},      // 眼睛向下瞪
    {.type = ACTION_MOTOR_CONTROL, .params.motor_control = {-80, -80, 400}},  // 后退
    {.type = ACTION_DELAY, .params.delay = {400}},
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_BOTH, 90}}     // 手臂复位
};

// 开心跳跃序列（手臂挥舞，履带跳跃，彩虹灯，开心表情）
static const action_cmd_t happy_sequence[] = {
    #if (USE_EYE_ENMOTION)
         {.type = ACTION_EXPRESSION, .params.expression = {EYES_EMOTION_HAPPY}},
    #else
        {.type = ACTION_EXPRESSION, .params.expression = {ANIMATION_EXCITED}},  // 开心表情
    #endif
    {.type = ACTION_LED_RAINBOW, .params.led_rainbow = {1000, true}},  // 彩虹灯
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_BOTH, 90}},  // 双手举起
    {.type = ACTION_MOTOR_CONTROL, .params.motor_control = {100, 100, 200}},  // 快速前进
    {.type = ACTION_DELAY, .params.delay = {200}},
    {.type = ACTION_MOTOR_CONTROL, .params.motor_control = {-100, -100, 200}}, // 快速后退（跳跃感）
    {.type = ACTION_DELAY, .params.delay = {200}},
    {.type = ACTION_MOTOR_CONTROL, .params.motor_control = {100, 100, 200}},  // 重复跳跃
    {.type = ACTION_DELAY, .params.delay = {200}},
    {.type = ACTION_MOTOR_CONTROL, .params.motor_control = {-100, -100, 200}},
    {.type = ACTION_DELAY, .params.delay = {200}},
    // {.type = ACTION_EYE_MOVE, .params.eye_move = {0.0f, 1.0f, 300}},     // 眼睛向上兴奋
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_BOTH, 180}}     // 手臂放下
};

// 伤心序列
static const action_cmd_t sad_sequence[] = {
    #if (USE_EYE_ENMOTION)
         {.type = ACTION_EXPRESSION, .params.expression = {EYES_EMOTION_SAD}},
    #else
        {.type = ACTION_EXPRESSION, .params.expression = {ANIMATION_SAD}},  // GIF_CRY是哭泣表情
    #endif
    {.type = ACTION_LED_BREATHE, .params.led_breathe = {0x0000FF, 500, false}},  // 蓝光闪烁呼吸
    // {.type = ACTION_EYE_MOVE, .params.eye_move = {0.0f, -1.0f, 400}},  // 眼睛向下悲伤
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_BOTH, 0}},  // 手臂下垂
    {.type = ACTION_MOTOR_CONTROL, .params.motor_control = {-50, -50, 600}},  // 缓慢后退
    {.type = ACTION_DELAY, .params.delay = {600}},
    // {.type = ACTION_EYE_MOVE, .params.eye_move = {0.0f, 0.0f, 300}}
};

// 可爱序列（眨眼，双手小挥，小跳，粉色灯呼吸）
static const action_cmd_t cute_sequence[] = {
    #if (USE_EYE_ENMOTION)
         {.type = ACTION_EXPRESSION, .params.expression = {EYES_EMOTION_HAPPY}},
    #else
        {.type = ACTION_EXPRESSION, .params.expression = {ANIMATION_EXCITED}},  // 开心表情
    #endif
    {.type = ACTION_LED_BREATHE, .params.led_breathe = {0x69FFB4, 1000, true}},  // 粉色呼吸
    // {.type = ACTION_EYE_WINK, .params.eye_wink = {500}},  // 眨眼
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_BOTH, 60}},  // 双手小举
    {.type = ACTION_DELAY, .params.delay = {300}},
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_BOTH, 30}},
    {.type = ACTION_DELAY, .params.delay = {300}},
    {.type = ACTION_MOTOR_CONTROL, .params.motor_control = {80, 80, 200}},  // 小跳前进
    {.type = ACTION_DELAY, .params.delay = {200}},
    {.type = ACTION_MOTOR_CONTROL, .params.motor_control = {-80, -80, 200}},  // 后退
    // 复位
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_BOTH, 90}},
    {.type = ACTION_DELAY, .params.delay = {300}}
};

// 舞蹈序列
static const action_cmd_t dance_routine_sequence[] = {
    {.type = ACTION_EYE_MOVE, .params.eye_move = {0.0f, 0.0f, 400}},
    {.type = ACTION_MOTOR_CONTROL, .params.motor_control = {100, 100, 500}},
    {.type = ACTION_LED_BREATHE, .params.led_breathe = {0x55FF55, 1000, true}},
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_LEFT, 100}},
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_RIGHT, 120}},
    {.type = ACTION_DELAY, .params.delay = {300}},  // 增加延时
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_LEFT, 120}},
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_RIGHT, 100}},
    {.type = ACTION_DELAY, .params.delay = {300}},
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_BOTH, 90}},
    {.type = ACTION_EYE_MOVE, .params.eye_move = {0.5f, 0.5f, 300}},
    {.type = ACTION_LED_SET, .params.led_set = {0xFF00FF, 0x00FFFF}},
    {.type = ACTION_DELAY, .params.delay = {500}},
    {.type = ACTION_EYE_MOVE, .params.eye_move = {-0.5f, 0.5f, 300}},
    {.type = ACTION_LED_SET, .params.led_set = {0xFFFF00, 0xFFA500}},
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_BOTH, 90}},
    {.type = ACTION_DELAY, .params.delay = {300}}
};

// 问候序列（挥右手，前进，微笑，绿灯呼吸）
static const action_cmd_t greeting_sequence[] = {
    #if (USE_EYE_ENMOTION)
         {.type = ACTION_EXPRESSION, .params.expression = {EYES_EMOTION_HAPPY}},
    #else
        {.type = ACTION_EXPRESSION, .params.expression = {ANIMATION_EXCITED}},  // 开心表情
    #endif
    {.type = ACTION_LED_BREATHE, .params.led_breathe = {0xFF0000, 3000, true}},  // 绿光呼吸
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_BOTH, 90}},  // 右手举起挥手
    {.type = ACTION_DELAY, .params.delay = {300}},
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_RIGHT, 0}},   // 右手放下
    // {.type = ACTION_MOTOR_CONTROL, .params.motor_control = {80, 80, 500}},  // 前进靠近
    // {.type = ACTION_EYE_MOVE, .params.eye_move = {0.0f, 0.5f, 400}},     // 眼睛向上看（友好）
    {.type = ACTION_DELAY, .params.delay = {300}},
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_RIGHT, 90}},  // 右手举起挥手
    {.type = ACTION_DELAY, .params.delay = {300}},
};

// 左上看序列
static const action_cmd_t left_eye_focus_sequence[] = {
    #if (USE_EYE_ENMOTION)
         {.type = ACTION_EXPRESSION, .params.expression = {EYES_EMOTION_FOCUS_TOP_LEFT}},
    #endif
    {.type = ACTION_EYE_MOVE, .params.eye_move = {1.0f, 1.0f, 300}},
    {.type = ACTION_LED_SET, .params.led_set = {0x000000, 0x0000FF}},
    {.type = ACTION_DELAY, .params.delay = {300}}
};

// 右上看序列
static const action_cmd_t right_eye_focus_sequence[] = {
    #if (USE_EYE_ENMOTION)
         {.type = ACTION_EXPRESSION, .params.expression = {EYES_EMOTION_FOCUS_TOP_RIGHT}},
    #endif
    {.type = ACTION_EYE_MOVE, .params.eye_move = {-1.0f, 1.0f, 300}},
    {.type = ACTION_LED_SET, .params.led_set = {0x0000FF, 0x000000}},
    {.type = ACTION_DELAY, .params.delay = {300}}
};

// 你别过来！
static const action_cmd_t combo_action_sequence[] = {
    #if (USE_EYE_ENMOTION)
         {.type = ACTION_EXPRESSION, .params.expression = {EYES_EMOTION_SUSPICIOUS}},
    #else
        {.type = ACTION_EXPRESSION, .params.expression = {ANIMATION_DISDAIN}},
    #endif
    {.type = ACTION_LED_BREATHE, .params.led_breathe = {0x55FF55, 1000, true}},
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_BOTH, 180}},
    {.type = ACTION_MOTOR_CONTROL, .params.motor_control = {100, 100, 200}},
    {.type = ACTION_DELAY, .params.delay = {200}},
    {.type = ACTION_MOTOR_CONTROL, .params.motor_control = {100, -100, 150}},
    {.type = ACTION_DELAY, .params.delay = {150}},
    {.type = ACTION_MOTOR_CONTROL, .params.motor_control = {-100, 100, 150}},
};

// 睡觉序列（眼睛闭合，手臂下垂，暗蓝灯呼吸）
static const action_cmd_t sleep_sequence[] = {
    {.type = ACTION_LED_BREATHE, .params.led_breathe = {0x000033, 2000, true}},  // 暗蓝呼吸（渐暗）
    {.type = ACTION_EYE_MOVE, .params.eye_move = {0.0f, -1.0f, 500}},    // 眼睛向下闭合
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_BOTH, 0}},    // 手臂下垂
    {.type = ACTION_DELAY, .params.delay = {1000}}                        // 保持休息
};

// 巡逻序列（眼睛扫描，履带转圈，白灯呼吸）
static const action_cmd_t patrol_sequence[] = {
    {.type = ACTION_LED_BREATHE, .params.led_breathe = {0xFFFFFF, 1500, false}},  // 白光呼吸（不对称，模拟扫描）
    {.type = ACTION_EYE_MOVE, .params.eye_move = {1.0f, 0.0f, 400}},     // 眼睛向右看
    {.type = ACTION_MOTOR_CONTROL, .params.motor_control = {80, -80, 500}},  // 右转
     {.type = ACTION_DELAY, .params.delay = {500}},
    {.type = ACTION_EYE_MOVE, .params.eye_move = {-1.0f, 0.0f, 400}},    // 眼睛向左看
    {.type = ACTION_MOTOR_CONTROL, .params.motor_control = {-80, 80, 500}},  // 左转
    {.type = ACTION_DELAY, .params.delay = {500}},
    {.type = ACTION_MOTOR_CONTROL, .params.motor_control = {80, 80, 300}},   // 前进一段
    {.type = ACTION_EYE_MOVE, .params.eye_move = {0.0f, 0.0f, 300}}      // 眼睛复位
};


// 点头同意序列（眼睛向下点头，绿灯，手臂轻举）
static const action_cmd_t nod_agree_sequence[] = {
    #if (USE_EYE_ENMOTION)
         {.type = ACTION_EXPRESSION, .params.expression = {EYES_EMOTION_FOCUS_BELOW_CENTER}},
    #endif
    {.type = ACTION_LED_SET, .params.led_set = {0x00FF00, 0x00FF00}},  // 绿灯
    {.type = ACTION_EYE_MOVE, .params.eye_move = {0.0f, -0.5f, 300}},  // 眼睛向下点头
    {.type = ACTION_DELAY, .params.delay = {300}},
    {.type = ACTION_EYE_MOVE, .params.eye_move = {0.0f, 0.0f, 300}},   // 眼睛复位
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_BOTH, 45}}, // 手臂轻举表示同意
    {.type = ACTION_DELAY, .params.delay = {500}},
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_BOTH, 90}}   // 手臂复位
};

// 摇头拒绝序列（眼睛左右摇，红灯，手臂交叉）
static const action_cmd_t shake_no_sequence[] = 
{
    #if (USE_EYE_ENMOTION)
         {.type = ACTION_EXPRESSION, .params.expression = {EYES_EMOTION_FOCUS_BELOW_CENTER}},
    #else
        {.type = ACTION_EXPRESSION, .params.expression = {ANIMATION_DISDAIN}},
    #endif
    {.type = ACTION_LED_SET, .params.led_set = {0xFF0000, 0xFF0000}},  // 红灯
    {.type = ACTION_EYE_MOVE, .params.eye_move = {0.5f, 0.0f, 200}},   // 眼睛向右
    {.type = ACTION_DELAY, .params.delay = {200}},
    {.type = ACTION_EYE_MOVE, .params.eye_move = {-0.5f, 0.0f, 200}},  // 眼睛向左
    {.type = ACTION_DELAY, .params.delay = {200}},
    {.type = ACTION_EYE_MOVE, .params.eye_move = {0.0f, 0.0f, 200}},   // 复位
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_LEFT, 120}}, // 手臂交叉拒绝
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_RIGHT, 60}},
    {.type = ACTION_DELAY, .params.delay = {500}}
};

// 拥抱序列（手臂合拢，前进，橙灯，微笑）
static const action_cmd_t hug_sequence[] = {
    #if (USE_EYE_ENMOTION)
         {.type = ACTION_EXPRESSION, .params.expression = {EYES_EMOTION_HAPPY}},
    #else
        {.type = ACTION_EXPRESSION, .params.expression = {ANIMATION_EXCITED}},  // 开心表情
    #endif
    {.type = ACTION_LED_BREATHE, .params.led_breathe = {0xFFA500, 1000, true}},  // 温暖橙光呼吸
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_BOTH, 0}},   // 手臂向前
    {.type = ACTION_DELAY, .params.delay = {1000}},
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_BOTH, 30}},  // 手臂合拢模拟拥抱
    {.type = ACTION_MOTOR_CONTROL, .params.motor_control = {60, 60, 400}},  // 前进靠近
    // {.type = ACTION_EYE_MOVE, .params.eye_move = {0.0f, 0.5f, 300}},     // 眼睛向上温暖
    {.type = ACTION_DELAY, .params.delay = {600}},
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_BOTH, 90}}    // 手臂复位
};

// 挥手告别序列（挥左手，后退，蓝灯）
static const action_cmd_t wave_goodbye_sequence[] = 
{
    #if (USE_EYE_ENMOTION)
         {.type = ACTION_EXPRESSION, .params.expression = {EYES_EMOTION_SAD}},
    #endif
    {.type = ACTION_LED_SET, .params.led_set = {0x0000FF, 0x0000FF}},  // 蓝灯
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_LEFT, 90}}, // 左手举起挥手
    {.type = ACTION_DELAY, .params.delay = {300}},
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_LEFT, 45}},
    {.type = ACTION_DELAY, .params.delay = {300}},
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_LEFT, 90}},
    {.type = ACTION_MOTOR_CONTROL, .params.motor_control = {-60, -60, 500}},  // 后退
    {.type = ACTION_EYE_MOVE, .params.eye_move = {0.0f, 0.0f, 300}},     // 眼睛注视
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_BOTH, 90}}    // 复位
};

// 思考序列（眼睛上移，手臂托腮，白灯呼吸）
static const action_cmd_t thinking_sequence[] = 
{
    #if (USE_EYE_ENMOTION)
         {.type = ACTION_EXPRESSION, .params.expression = {EYES_EMOTION_FOCUS_TOP_CENTER}},
    #endif
    {.type = ACTION_LED_BREATHE, .params.led_breathe = {0xFFFFFF, 1500, true}},  // 白光呼吸
    {.type = ACTION_EYE_MOVE, .params.eye_move = {0.2f, 0.8f, 400}},   // 眼睛上移侧看
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_RIGHT, 90}}, // 右手托腮模拟
    {.type = ACTION_DELAY, .params.delay = {800}},
    {.type = ACTION_EYE_MOVE, .params.eye_move = {0.0f, 0.0f, 300}},   // 复位
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_RIGHT, 0}}
};

// 哭泣序列（眼睛向下，蓝灯闪烁，手臂下垂，后退）
static const action_cmd_t cry_sequence[] = {
    #if (USE_EYE_ENMOTION)
         {.type = ACTION_EXPRESSION, .params.expression = {EYES_EMOTION_SAD}},
    #else
        {.type = ACTION_EXPRESSION, .params.expression = {ANIMATION_SAD}},  // GIF_CRY是哭泣表情
    #endif
    {.type = ACTION_LED_BREATHE, .params.led_breathe = {0x0000FF, 500, false}},  // 蓝光闪烁呼吸
    {.type = ACTION_EYE_MOVE, .params.eye_move = {0.0f, -1.0f, 400}},  // 眼睛向下悲伤
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_BOTH, 0}},  // 手臂下垂
    {.type = ACTION_MOTOR_CONTROL, .params.motor_control = {-50, -50, 600}},  // 缓慢后退
    {.type = ACTION_DELAY, .params.delay = {600}},
    {.type = ACTION_EYE_MOVE, .params.eye_move = {0.0f, 0.0f, 300}}
};

// 鄙视序列（眼睛斜视，手臂耸肩，红灯闪烁，转身离开）
static const action_cmd_t disdain_sequence[] = {
    #if (USE_EYE_ENMOTION)
         {.type = ACTION_EXPRESSION, .params.expression = {EYES_EMOTION_SUSPICIOUS}},
    #else
        {.type = ACTION_EXPRESSION, .params.expression = {ANIMATION_DISDAIN}},
    #endif
    {.type = ACTION_LED_BLINK, .params.led_blink = {0xFF0000, 500, false}},  // 红灯闪烁
    {.type = ACTION_EYE_MOVE, .params.eye_move = {0.8f, 0.5f, 400}},  // 眼睛斜视右上
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_RIGHT, 90}},  // 右手耸肩
    {.type = ACTION_DELAY, .params.delay = {400}},
    {.type = ACTION_MOTOR_CONTROL, .params.motor_control = {80, -80, 500}},  // 右转离开
    {.type = ACTION_DELAY, .params.delay = {500}},
    // 复位
    {.type = ACTION_EYE_MOVE, .params.eye_move = {0.0f, 0.0f, 300}},
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_RIGHT, 0}},
    {.type = ACTION_MOTOR_CONTROL, .params.motor_control = {0, 0, 0}}
};

// 胜利序列（双手举起，旋转，绿灯闪烁）
static const action_cmd_t victory_sequence[] = {
    #if (USE_EYE_ENMOTION)
         {.type = ACTION_EXPRESSION, .params.expression = {EYES_EMOTION_HAPPY}},
    #else
        {.type = ACTION_EXPRESSION, .params.expression = {ANIMATION_EXCITED}},  // 开心表情
    #endif
    {.type = ACTION_LED_BLINK, .params.led_blink = {0x00FF00, 300, true}},  // 绿灯闪烁
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_BOTH, 90}},  // 双手举起
    {.type = ACTION_MOTOR_CONTROL, .params.motor_control = {100, -100, 800}},  // 旋转庆祝
    {.type = ACTION_DELAY, .params.delay = {800}},
    {.type = ACTION_EYE_MOVE, .params.eye_move = {0.0f, 1.0f, 300}},  // 眼睛向上
    // 复位
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_BOTH, 0}},
    {.type = ACTION_MOTOR_CONTROL, .params.motor_control = {0, 0, 0}},
    {.type = ACTION_EYE_MOVE, .params.eye_move = {0.0f, 0.0f, 300}}
};

// 困惑序列（摇头，眼睛乱转，黄灯，手臂耸肩）
static const action_cmd_t confused_sequence[] = {
    #if (USE_EYE_ENMOTION)
         {.type = ACTION_EXPRESSION, .params.expression = {EYES_EMOTION_FOCUS_TOP_CENTER}},
    #endif
    {.type = ACTION_LED_SET, .params.led_set = {0xFFFF00, 0xFFFF00}},  // 黄灯
    {.type = ACTION_EYE_MOVE, .params.eye_move = {0.5f, 0.0f, 200}},  // 右看
    {.type = ACTION_DELAY, .params.delay = {200}},
    {.type = ACTION_EYE_MOVE, .params.eye_move = {-0.5f, 0.0f, 200}},  // 左看
    {.type = ACTION_DELAY, .params.delay = {200}},
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_BOTH, 90}},  // 耸肩
    {.type = ACTION_DELAY, .params.delay = {400}},
    // 复位
    {.type = ACTION_EYE_MOVE, .params.eye_move = {0.0f, 0.0f, 300}},
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_BOTH, 0}}
};

// 害怕扩展序列（后退，手臂护胸，蓝灯闪烁）
static const action_cmd_t fear_extended_sequence[] = {
    #if (USE_EYE_ENMOTION)
        {.type = ACTION_EXPRESSION, .params.expression = {EYES_EMOTION_AMAZED}},
    #else
        {.type = ACTION_EXPRESSION, .params.expression = {ANIMATION_FEAR}},  // 惊讶表情
    #endif
    {.type = ACTION_LED_BLINK, .params.led_blink = {0x0000FF, 200, false}},  // 蓝灯快速闪烁
    {.type = ACTION_EYE_MOVE, .params.eye_move = {0.0f, 1.0f, 200}},  // 眼睛睁大
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_BOTH, 120}},  // 手臂护胸（向上偏后）
    {.type = ACTION_MOTOR_CONTROL, .params.motor_control = {-100, -100, 500}},  // 快速后退
    {.type = ACTION_DELAY, .params.delay = {500}},
    // 复位
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_BOTH, 0}},
    {.type = ACTION_MOTOR_CONTROL, .params.motor_control = {0, 0, 0}},
    {.type = ACTION_EYE_MOVE, .params.eye_move = {0.0f, 0.0f, 300}}
};


/************************* default actions *******************/
static const action_cmd_t eyes_default_sequence[] = {
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_BOTH, 180}},
    {.type = ACTION_LED_OFF}
};

static const action_cmd_t eyes_first_wakeup_sequence[] = {
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_BOTH, 90}},
    {.type = ACTION_LED_BREATHE, .params.led_breathe = {0xFFFFFF, 1000, true}}
};

static const action_cmd_t music_default_sequence[] = {
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_BOTH, 90}},
    {.type = ACTION_LED_RAINBOW, .params.led_rainbow = {12000, true}}
};

static const action_cmd_t lucky_cat_default_sequence[] = {
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_LEFT, 180}},
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_RIGHT, 90}},
    {.type = ACTION_LED_OFF}
};

static const action_cmd_t camera_default_sequence[] = {
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_BOTH, 135}},
    {.type = ACTION_LED_OFF}
};

static const action_cmd_t clock_default_sequence[] = {
    {.type = ACTION_SERVO_MOVE, .params.servo_move = {SERVO_BOTH, 90}},
    {.type = ACTION_LED_OFF}
};

static const action_cmd_t general_default_sequence[] = {
    {.type = ACTION_DELAY, .params.delay = {500}},
};

/************************* default actions end *******************/

/* ------------------------- 序列定义表 ------------------------- */
static const action_sequence_t sequences[SEQUENCE_COUNT] = {
    [SEQUENCE_DANCE_ROUTINE] = {.actions = dance_routine_sequence,.count = sizeof(dance_routine_sequence) / sizeof(action_cmd_t)},
    [SEQUENCE_LEFT_EYE_FOCUS] = {.actions = left_eye_focus_sequence,.count = sizeof(left_eye_focus_sequence) / sizeof(action_cmd_t)},
    [SEQUENCE_RIGHT_EYE_FOCUS] = {.actions = right_eye_focus_sequence,.count = sizeof(right_eye_focus_sequence) / sizeof(action_cmd_t)},
    [SEQUENCE_COMBO_ACTION] = {.actions = combo_action_sequence, .count = sizeof(combo_action_sequence) / sizeof(action_cmd_t)},
    [SEQUENCE_GREETING] = { .actions = greeting_sequence, .count = sizeof(greeting_sequence) / sizeof(action_cmd_t) },
    [SEQUENCE_ANGRY] = { .actions = angry_sequence, .count = sizeof(angry_sequence) / sizeof(action_cmd_t) },
    [SEQUENCE_HAPPY] = { .actions = happy_sequence, .count = sizeof(happy_sequence) / sizeof(action_cmd_t) },
    [SEQUENCE_SLEEP] = { .actions = sleep_sequence, .count = sizeof(sleep_sequence) / sizeof(action_cmd_t) },
    [SEQUENCE_SURPRISED] = { .actions = surprised_sequence, .count = sizeof(surprised_sequence) / sizeof(action_cmd_t) },
    [SEQUENCE_PATROL] = { .actions = patrol_sequence, .count = sizeof(patrol_sequence) / sizeof(action_cmd_t) },
    [SEQUENCE_NOD_AGREE] = { .actions = nod_agree_sequence, .count = sizeof(nod_agree_sequence) / sizeof(action_cmd_t) },
    [SEQUENCE_SHAKE_NO] = { .actions = shake_no_sequence, .count = sizeof(shake_no_sequence) / sizeof(action_cmd_t) },
    [SEQUENCE_HUG] = { .actions = hug_sequence, .count = sizeof(hug_sequence) / sizeof(action_cmd_t) },
    [SEQUENCE_WAVE_GOODBYE] = { .actions = wave_goodbye_sequence, .count = sizeof(wave_goodbye_sequence) / sizeof(action_cmd_t) },
    [SEQUENCE_THINKING] = { .actions = thinking_sequence, .count = sizeof(thinking_sequence) / sizeof(action_cmd_t) },
    [SEQUENCE_CRY] = { .actions = cry_sequence, .count = sizeof(cry_sequence) / sizeof(action_cmd_t) },
    [SEQUENCE_SAD] = { .actions = sad_sequence, .count = sizeof(sad_sequence) / sizeof(action_cmd_t) },
    [SEQUENCE_DISDAIN] = {.actions = disdain_sequence, .count = sizeof(disdain_sequence) / sizeof(action_cmd_t)},
    [SEQUENCE_CUTE] = {.actions = cute_sequence, .count = sizeof(cute_sequence) / sizeof(action_cmd_t)},
    [SEQUENCE_VICTORY] = {.actions = victory_sequence, .count = sizeof(victory_sequence) / sizeof(action_cmd_t)},
    [SEQUENCE_CONFUSED] = {.actions = confused_sequence, .count = sizeof(confused_sequence) / sizeof(action_cmd_t)},
    [SEQUENCE_FEAR_EXTENDED] = {.actions = fear_extended_sequence, .count = sizeof(fear_extended_sequence) / sizeof(action_cmd_t)},

    [SEQUENCE_EYES_DEFAULT] = {.actions = eyes_default_sequence,.count = sizeof(eyes_default_sequence) / sizeof(action_cmd_t)},
    [SEQUENCE_EYES_FIRST_WAKEUP] = {.actions = eyes_first_wakeup_sequence,.count = sizeof(eyes_first_wakeup_sequence) / sizeof(action_cmd_t)},
    [SEQUENCE_MUSIC_DEFAULT] = {.actions = music_default_sequence,.count = sizeof(music_default_sequence) / sizeof(action_cmd_t)},
    [SEQUENCE_LUCKY_CAT_DEFAULT] = {.actions = lucky_cat_default_sequence,.count = sizeof(lucky_cat_default_sequence) / sizeof(action_cmd_t)},
    [SEQUENCE_CAMERA_DEFAULT] = {.actions = camera_default_sequence,.count = sizeof(camera_default_sequence) / sizeof(action_cmd_t)},
    [SEQUENCE_CLOCK_DEFAULT] = {.actions = clock_default_sequence,.count = sizeof(clock_default_sequence) / sizeof(action_cmd_t)},
    [SEQUENCE_GENERAL_DEFAULT] = {.actions = general_default_sequence,.count = sizeof(general_default_sequence) / sizeof(action_cmd_t)},

};

/* ======================= 动作队列管理（RAM中） ======================= */
// 动作队列和状态机
#define ACTION_QUEUE_SIZE 20
static action_cmd_t action_queue[ACTION_QUEUE_SIZE];
static uint8_t queue_head = 0;
static uint8_t queue_tail = 0;
static uint8_t queue_count = 0;

// 当前动作状态
static action_cmd_t current_action = {ACTION_NONE};
static uint32_t action_start_time = 0;

void action_sequences_reset(void) {
    queue_head = 0;
    queue_tail = 0;
    queue_count = 0;
    current_action.type = ACTION_NONE;

    LOG_DBG("Action queue reset");
}

bool action_sequences_enqueue_action(action_cmd_t cmd) {
    if (queue_count >= ACTION_QUEUE_SIZE) {
        LOG_WRN("Action queue full!");
        return false;
    }
    
    action_queue[queue_tail] = cmd;
    queue_tail = (queue_tail + 1) % ACTION_QUEUE_SIZE;
    queue_count++;

    return true;
}

bool action_sequences_enqueue(sequence_type_t seq_type) {
    if (seq_type < 0 || seq_type >= SEQUENCE_COUNT) {
        LOG_ERR("Invalid sequence type: %d", seq_type);
        return false;
    }
    
    const action_sequence_t *seq = &sequences[seq_type];
    bool success = true;
    
    for (uint8_t i = 0; i < seq->count; i++) {
        if (!action_sequences_enqueue_action(seq->actions[i])) {
            success = false;
            break;
        }
    }
    
    LOG_DBG("Enqueued sequence: %d (%d actions)", seq_type, success ? seq->count : 0);
    return success;
}

bool action_sequences_enqueue_custom(const action_cmd_t *actions, uint8_t count) {
    if (!actions || count == 0) {
        return false;
    }
    
    bool success = true;
    for (uint8_t i = 0; i < count; i++) {
        if (!action_sequences_enqueue_action(actions[i])) {
            success = false;
            break;
        }
    }
    
    LOG_DBG("Enqueued custom sequence (%d/%d actions)", success ? count : 0, count);
    return success;
}

action_type_t action_sequences_get_current_action(void) {
    return current_action.type;
}

// 检查队列是否为空
bool action_sequences_queue_is_empty() {
    return queue_count == 0;
}

void action_sequences_process(void) 
{
    // 当前没有动作时从队列获取新动作
    if (current_action.type == ACTION_NONE && queue_count > 0) 
    {
        current_action = action_queue[queue_head];
        queue_head = (queue_head + 1) % ACTION_QUEUE_SIZE;
        queue_count--;
        action_start_time = k_uptime_get_32();
        LOG_DBG("Start action: %d", current_action.type);
        
        // 执行动作初始化
        switch (current_action.type) 
        {
            // case ACTION_EYE_WINK:
            //     eye_controller_blink(&control, 1000);
            //     break;
            // case ACTION_EYE_MOVE:
            //     eye_controller_set_target_position(
            //         &control,
            //         current_action.params.eye_move.x,
            //         current_action.params.eye_move.y,
            //         current_action.params.eye_move.duration
            //     );
            //     break;
                
            case ACTION_LED_SET:
                led_set_rgb(
                    current_action.params.led_set.left_color,
                    current_action.params.led_set.right_color
                );
                break;
            case ACTION_LED_BREATHE:
                led_set_breathing(
                    current_action.params.led_breathe.color,
                    current_action.params.led_breathe.period,
                    current_action.params.led_breathe.symmetric
                );
                break;
            case ACTION_LED_BLINK:
                led_set_blink(
                    current_action.params.led_breathe.color,
                    current_action.params.led_breathe.period,
                    current_action.params.led_breathe.symmetric
                );
                break;
            case ACTION_LED_RAINBOW:
                led_set_rainbow(
                    current_action.params.led_rainbow.period_ms, 
                    current_action.params.led_rainbow.symmetric
                );
                break;
            case ACTION_LED_OFF:
                led_off();
                break;
                
            case ACTION_SERVO_MOVE:
                servo_set_angle(
                    current_action.params.servo_move.servo,
                    current_action.params.servo_move.angle
                );
                break;
                
            case ACTION_MOTOR_CONTROL:
                motor_move_smooth(
                    current_action.params.motor_control.left_speed,
                    current_action.params.motor_control.right_speed,
                    current_action.params.motor_control.duration
                );
                break;
                
            case ACTION_EXPRESSION:
            #if (USE_EYE_ENMOTION)
                eye_controller_apply_emotion(&control, current_action.params.expression.animation);
                screen_set_current(SCREEN_EYES);
            #else
                switch_gif(current_action.params.expression.animation);
                screen_set_current(SCREEN_EMOJI_GIF);
            #endif
                break;
                
            case ACTION_DELAY:
                // 延时动作无需立即执行
                break;
                
            default:
                break;
        }
    }
    
    // 处理当前动作
    if (current_action.type != ACTION_NONE) 
    {
        bool action_completed = false;
        uint32_t elapsed = k_uptime_get_32() - action_start_time;
        
        switch (current_action.type) 
        {
            case ACTION_DELAY:
                action_completed = (elapsed >= current_action.params.delay.delay_ms);
                break;
                
            case ACTION_EYE_MOVE:
                action_completed = (elapsed >= current_action.params.eye_move.duration);
                break;
            case ACTION_MOTOR_CONTROL:
                action_completed = (elapsed >= current_action.params.motor_control.duration);
                break;
            default:
                action_completed = true;
                break;
        }
        
        if (action_completed) 
        {
            LOG_DBG("Action completed: %d", current_action.type);
            current_action.type = ACTION_NONE;
        }
    }
}


/* ======================== 辅助函数 ======================== */

static void action_cmd_handler(const struct shell *sh, size_t argc, char **argv)
{
	if (argc < 2) {
        shell_print(sh, "para error");
        return;
    }

    char *end;
    long number = strtol(argv[1], &end, 10);
    if (*end != '\0') {
        shell_print(sh, "para error: %s", argv[1]);
        return;
    }

    shell_print(sh, "action: %ld", number);
    action_sequences_enqueue(number);
}


// 检查字符串是否为有效的十六进制颜色
static bool is_valid_hex_color(const char *hex) {
    if (strlen(hex) != 6) return false;
    for (int i = 0; i < 6; i++) {
        char c = hex[i];
        if (!(('0' <= c && c <= '9') || 
              ('A' <= c && c <= 'F') || 
              ('a' <= c && c <= 'f'))) {
            return false;
        }
    }
    return true;
}

// 十六进制字符串转uint32_t
static uint32_t hex_str_to_uint32(const char *hex_str) {
    uint32_t color;
    sscanf(hex_str, "%06x", &color);  // 读取6位十六进制
    return color;
}

/* ======================== 动作处理函数 ======================== */
// 眼球移动
static int cmd_eye_move(const struct shell *sh, size_t argc, char **argv) {
    if (argc != 4) {
        shell_error(sh, "用法: eye_move <x> <y> <duration_ms>");
        return -EINVAL;
    }

    action_cmd_t cmd = {
        .type = ACTION_EYE_MOVE,
        .params.eye_move = {
            .x = strtof(argv[1], NULL),
            .y = strtof(argv[2], NULL),
            .duration = (uint16_t)strtoul(argv[3], NULL, 10)
        }
    };
    
    if (!action_sequences_enqueue_action(cmd)) {
        shell_error(sh, "队列已满!");
        return -ENOMEM;
    }
    
    shell_print(sh, "已添加眼球移动: x=%.1f, y=%.1f, 时长=%ums", 
               cmd.params.eye_move.x, 
               cmd.params.eye_move.y, 
               cmd.params.eye_move.duration);
    return 0;
}

// 设置LED颜色
static int cmd_led_set(const struct shell *sh, size_t argc, char **argv) {
    if (argc != 3) {
        shell_error(sh, "用法: led_set <左眼颜色(6位十六进制)> <右眼颜色(6位十六进制)>");
        return -EINVAL;
    }

    const char *left_hex = argv[1];
    const char *right_hex = argv[2];
    
    if (!is_valid_hex_color(left_hex) || !is_valid_hex_color(right_hex)) {
        shell_error(sh, "颜色格式无效! 使用6位十六进制(例如FF00FF)");
        return -EINVAL;
    }

    action_cmd_t cmd = {
        .type = ACTION_LED_SET,
        .params.led_set = {
            .left_color = hex_str_to_uint32(left_hex),
            .right_color = hex_str_to_uint32(right_hex)
        }
    };
    
    if (!action_sequences_enqueue_action(cmd)) {
        shell_error(sh, "队列已满!");
        return -ENOMEM;
    }
    
    shell_print(sh, "已设置LED: 左眼=0x%06X, 右眼=0x%06X", 
               cmd.params.led_set.left_color,
               cmd.params.led_set.right_color);
    return 0;
}

// LED呼吸效果
static int cmd_led_breathe(const struct shell *sh, size_t argc, char **argv) {
    if (argc != 4) {
        shell_error(sh, "用法: led_breathe <颜色(6位十六进制)> <周期(ms)> <同步模式(0:异步, 1:同步)>");
        return -EINVAL;
    }

    const char *color_hex = argv[1];
    if (!is_valid_hex_color(color_hex)) {
        shell_error(sh, "颜色格式无效! 使用6位十六进制(例如FF00FF)");
        return -EINVAL;
    }
    
    long symmetric = strtol(argv[3], NULL, 10);
    if (symmetric != 0 && symmetric != 1) {
        shell_error(sh, "同步模式必须是0或1!");
        return -EINVAL;
    }

    action_cmd_t cmd = {
        .type = ACTION_LED_BREATHE,
        .params.led_breathe = {
            .color = hex_str_to_uint32(color_hex),
            .period = (uint16_t)strtoul(argv[2], NULL, 10),
            .symmetric = (bool)symmetric
        }
    };
    
    if (!action_sequences_enqueue_action(cmd)) {
        shell_error(sh, "队列已满!");
        return -ENOMEM;
    }
    
    shell_print(sh, "已添加呼吸效果: 颜色=0x%06X, 周期=%ums, 同步=%s", 
               cmd.params.led_breathe.color,
               cmd.params.led_breathe.period,
               cmd.params.led_breathe.symmetric ? "是" : "否");
    return 0;
}

// LED彩虹效果
static int cmd_led_rainbow(const struct shell *sh, size_t argc, char **argv) {
    if (argc != 3) {
        shell_error(sh, "用法: led_rainbow <周期(ms)> <同步模式(0:异步, 1:同步)>");
        return -EINVAL;
    }
    
    long symmetric = strtol(argv[2], NULL, 10);
    if (symmetric != 0 && symmetric != 1) {
        shell_error(sh, "同步模式必须是0或1!");
        return -EINVAL;
    }

    action_cmd_t cmd = {
        .type = ACTION_LED_RAINBOW,
        .params.led_rainbow = {
            .period_ms = (uint16_t)strtoul(argv[1], NULL, 10),
            .symmetric = (bool)symmetric
        }
    };
    
    if (!action_sequences_enqueue_action(cmd)) {
        shell_error(sh, "队列已满!");
        return -ENOMEM;
    }
    
    shell_print(sh, "已添加彩虹效果: 周期=%ums, 同步=%s", 
               cmd.params.led_rainbow.period_ms,
               cmd.params.led_rainbow.symmetric ? "是" : "否");
    return 0;
}

// 舵机控制
static int cmd_servo_move(const struct shell *sh, size_t argc, char **argv) {
    if (argc != 3) {
        shell_error(sh, "用法: servo_move <舵机(0:左,1:右,2:双)> <角度(0-180)>");
        return -EINVAL;
    }
    
    long servo = strtol(argv[1], NULL, 10);
    if (servo < 0 || servo > 2) {
        shell_error(sh, "舵机选择无效! (0-左,1-右,2-双)");
        return -EINVAL;
    }
    
    long angle = strtol(argv[2], NULL, 10);
    if (angle < 0 || angle > 180) {
        shell_error(sh, "角度范围无效! (0-180)");
        return -EINVAL;
    }

    action_cmd_t cmd = {
        .type = ACTION_SERVO_MOVE,
        .params.servo_move = {
            .servo = (servo_select_t)servo,
            .angle = (uint8_t)angle
        }
    };
    
    if (!action_sequences_enqueue_action(cmd)) {
        shell_error(sh, "队列已满!");
        return -ENOMEM;
    }
    
    const char *servo_str[] = {"左舵机", "右舵机", "双舵机"};
    shell_print(sh, "已添加舵机控制: %s → %u度", 
               servo_str[servo], cmd.params.servo_move.angle);
    return 0;
}

// 电机控制
static int cmd_motor_control(const struct shell *sh, size_t argc, char **argv) {
    if (argc != 4) {
        shell_error(sh, "用法: motor_control <x方向> <y方向> <时长(ms)>");
        return -EINVAL;
    }

    action_cmd_t cmd = {
        .type = ACTION_MOTOR_CONTROL,
        .params.motor_control = {
            .left_speed = (int16_t)strtoul(argv[1], NULL, 10),
            .right_speed = (int16_t)strtoul(argv[2], NULL, 10),
            .duration = (uint16_t)strtoul(argv[3], NULL, 10)
        }
    };
    
    if (!action_sequences_enqueue_action(cmd)) {
        shell_error(sh, "队列已满!");
        return -ENOMEM;
    }
    return 0;
}

// 表情动画
static int cmd_expression(const struct shell *sh, size_t argc, char **argv) {
    if (argc != 2) {
        shell_error(sh, "用法: expression <动画ID>");
        return -EINVAL;
    }
    
    long anim_id = strtol(argv[1], NULL, 10);
    if (anim_id < 0) {
        shell_error(sh, "动画ID无效!");
        return -EINVAL;
    }

    action_cmd_t cmd = {
        .type = ACTION_EXPRESSION,
        .params.expression = {
            .animation = (gif_animation_type_t)anim_id
        }
    };
    
    if (!action_sequences_enqueue_action(cmd)) {
        shell_error(sh, "队列已满!");
        return -ENOMEM;
    }
    
    return 0;
}

// 延时
static int cmd_delay(const struct shell *sh, size_t argc, char **argv) {
    if (argc != 2) {
        shell_error(sh, "用法: delay <时长(ms)>");
        return -EINVAL;
    }
    
    uint32_t delay_ms = strtoul(argv[1], NULL, 10);
    if (delay_ms == 0) {
        shell_error(sh, "延时时间无效!");
        return -EINVAL;
    }

    action_cmd_t cmd = {
        .type = ACTION_DELAY,
        .params.delay = {
            .delay_ms = (uint16_t)delay_ms
        }
    };
    
    if (!action_sequences_enqueue_action(cmd)) {
        shell_error(sh, "队列已满!");
        return -ENOMEM;
    }

    return 0;
}

/* ======================== Shell命令注册 ======================== */
SHELL_STATIC_SUBCMD_SET_CREATE(
    sub_action_send,
    SHELL_CMD_ARG(eye_move, NULL, 
        "控制眼球移动\n参数: <x坐标> <y坐标> <时长(ms)>", 
        cmd_eye_move, 4, 0),
    SHELL_CMD_ARG(led_set, NULL, 
        "设置LED颜色\n参数: <左眼颜色(6位十六进制)> <右眼颜色(6位十六进制)>", 
        cmd_led_set, 3, 0),
    SHELL_CMD_ARG(led_breathe, NULL, 
        "设置呼吸效果\n参数: <颜色(6位十六进制)> <周期(ms)> <同步模式(0:异步,1:同步)>", 
        cmd_led_breathe, 4, 0),
    SHELL_CMD_ARG(led_rainbow, NULL, 
        "设置彩虹效果\n参数: <周期(ms)> <同步模式(0:异步,1:同步)>", 
        cmd_led_rainbow, 3, 0),
    SHELL_CMD_ARG(servo_move, NULL, 
        "控制舵机\n参数: <舵机(0:左,1:右,2:双)> <角度(0-180)>", 
        cmd_servo_move, 3, 0),
    SHELL_CMD_ARG(motor_control, NULL, 
        "控制电机\n参数: <x方向> <y方向> <时长(ms)>", 
        cmd_motor_control, 4, 0),
    SHELL_CMD_ARG(expression, NULL, 
        "播放表情动画\n参数: <动画ID>", 
        cmd_expression, 2, 0),
    SHELL_CMD_ARG(delay, NULL, 
        "添加延时\n参数: <时长(ms)>", 
        cmd_delay, 2, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(
    sub_action,
    SHELL_CMD(set, NULL, "执行预定义序列\n参数: <序列ID>", action_cmd_handler),
    SHELL_CMD(send, &sub_action_send, "发送自定义动作命令", NULL),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(action, &sub_action, "动作控制系统", NULL);



