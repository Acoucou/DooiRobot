#ifndef LED_H
#define LED_H

#include <zephyr/kernel.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 枚举定义特效类型
typedef enum {
    LED_EFFECT_RGB = 0,
    LED_EFFECT_BREATHING,
    LED_EFFECT_RAINBOW,
    LED_EFFECT_BLINK,
    LED_EFFECT_COUNT 
} led_fun_effect_t;

// 初始化LED系统
void led_init(void);

// 基础颜色设置（适配原有接口）
void led_set_rgb(int left_color, int right_color);

// 特效控制
void led_set_breathing(int color, uint16_t period_ms, bool symmetric);
void led_set_rainbow(uint16_t period_ms, bool symmetric);
void led_set_blink(int color, uint16_t period_ms, bool symmetric);
void led_off(void);

void led_random_set(led_fun_effect_t effect);

// 主循环处理函数
void led_process(void);

#ifdef __cplusplus
}
#endif

#endif /* LED_H */
