#ifndef __KEY_H
#define __KEY_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

// 按键状态结构体
struct button_state {
    const struct gpio_dt_spec *spec;
    struct gpio_callback cb;
    struct k_timer click_timer;
    struct k_work_delayable long_press_work;
    bool stable_state;
    bool processing_press;
    bool pending_event;
    bool long_press_triggered;
    uint8_t click_count;
    uint32_t last_change_time;
};

typedef enum button {
    BUTTON_LEFT,
    BUTTON_RIGHT,
}button_t;

// 按键事件类型
typedef enum button_event {
    BUTTON_PRESSED,
    BUTTON_RELEASED,
    BUTTON_LONG_PRESS,
    BUTTON_SINGLE_CLICK, 
    BUTTON_DOUBLE_CLICK,
    BUTTON_TRIPLE_CLICK,
}button_event_t;

int app_key_init(void);
void app_handle_button(button_t button, button_event_t event);

#endif // __KEY_H
