#include "key.h"
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

LOG_MODULE_REGISTER(app_key, LOG_LEVEL_DBG);

#define BUTTON_DEBOUNCE_TIME 20
#define LONG_PRESS_TIME      2500
#define CLICK_TIME           600

#define SW0_NODE DT_ALIAS(sw0)
#define SW1_NODE DT_ALIAS(sw1)
static const struct gpio_dt_spec k3 = GPIO_DT_SPEC_GET(SW0_NODE, gpios);
static const struct gpio_dt_spec k4 = GPIO_DT_SPEC_GET(SW1_NODE, gpios);

static struct button_state btn_k3_state;
static struct button_state btn_k4_state;


// ========== 事件处理 ==========
static void handle_button_event(struct button_state *btn, button_event_t event)
{
    if(btn == &btn_k3_state)
    {
        LOG_INF("Button K3 event: %d", event);
        app_handle_button(BUTTON_LEFT, event);
    }
    else if(btn == &btn_k4_state)
    {
        LOG_INF("Button K4 event: %d", event);
        app_handle_button(BUTTON_RIGHT, event);
    }
}

// ========== 长按处理 ==========
static void long_press_work_handler(struct k_work *work)
{
    struct button_state *state = CONTAINER_OF(work, struct button_state, long_press_work.work);

    if (gpio_pin_get_dt(state->spec) == 0) {
        state->click_count = 0;
        state->pending_event = false;
        state->long_press_triggered = true;
        handle_button_event(state, BUTTON_LONG_PRESS);
    }
}

// ========== 单击/双击判断 ==========
static void click_timeout(struct k_timer *timer)
{
    struct button_state *state = CONTAINER_OF(timer, struct button_state, click_timer);

    if (state->click_count == 1) 
    {
        handle_button_event(state, BUTTON_SINGLE_CLICK);
    } 
    else if (state->click_count == 2) 
    {
        handle_button_event(state, BUTTON_DOUBLE_CLICK);
    }
    else if (state->click_count == 3) 
    {
        handle_button_event(state, BUTTON_TRIPLE_CLICK);
    }

    state->click_count = 0;
}

// ========== 中断回调 ==========
static void button_pressed(const struct device *dev, 
                          struct gpio_callback *cb, 
                          uint32_t pins)
{
    struct button_state *state = CONTAINER_OF(cb, struct button_state, cb);
    bool current_state = (gpio_pin_get_dt(state->spec) == 0);
    uint32_t curr_time = k_uptime_get_32();

    if (curr_time - state->last_change_time < BUTTON_DEBOUNCE_TIME) return;
    state->last_change_time = curr_time;

    if (current_state && !state->stable_state) 
    {
        // 按下
        state->processing_press = true;
        state->pending_event = false;
        k_work_schedule(&state->long_press_work, K_MSEC(LONG_PRESS_TIME));
    } 
    else if (!current_state && state->stable_state) 
    {
        // 弹起
        if (state->processing_press && !state->long_press_triggered) 
        {
            k_work_cancel_delayable(&state->long_press_work);
            state->click_count++;
            k_timer_start(&state->click_timer, K_MSEC(CLICK_TIME), K_NO_WAIT);
        }
        state->processing_press = false;
        state->long_press_triggered = false;
    }

    state->stable_state = current_state;
}

// ========== 初始化函数 ==========
int app_key_init(void)
{
    int ret;

    // 初始化按键 K3
    if (!device_is_ready(k3.port)) {
        LOG_ERR("K3 not ready");
        return -ENODEV;
    }
    ret = gpio_pin_configure_dt(&k3, GPIO_INPUT | GPIO_PULL_UP);
    ret |= gpio_pin_interrupt_configure_dt(&k3, GPIO_INT_EDGE_BOTH);
    if (ret != 0) return ret;

    gpio_init_callback(&btn_k3_state.cb, button_pressed, BIT(k3.pin));
    gpio_add_callback(k3.port, &btn_k3_state.cb);

    btn_k3_state.spec = &k3;
    btn_k3_state.stable_state = gpio_pin_get_dt(&k3) == 0;
    btn_k3_state.last_change_time = 0;
    btn_k3_state.processing_press = false;
    btn_k3_state.click_count = 0;

    k_timer_init(&btn_k3_state.click_timer, click_timeout, NULL);
    k_work_init_delayable(&btn_k3_state.long_press_work, long_press_work_handler);

    // 初始化按键 K4
    if (!device_is_ready(k4.port)) {
        LOG_ERR("K4 not ready");
        return -ENODEV;
    }
    ret = gpio_pin_configure_dt(&k4, GPIO_INPUT | GPIO_PULL_UP);
    ret |= gpio_pin_interrupt_configure_dt(&k4, GPIO_INT_EDGE_BOTH);
    if (ret != 0) return ret;

    gpio_init_callback(&btn_k4_state.cb, button_pressed, BIT(k4.pin));
    gpio_add_callback(k4.port, &btn_k4_state.cb);

    btn_k4_state.spec = &k4;
    btn_k4_state.stable_state = gpio_pin_get_dt(&k4) == 0;
    btn_k4_state.last_change_time = 0;
    btn_k4_state.processing_press = false;
    btn_k4_state.click_count = 0;

    k_timer_init(&btn_k4_state.click_timer, click_timeout, NULL);
    k_work_init_delayable(&btn_k4_state.long_press_work, long_press_work_handler);

    LOG_INF("Button module initialized");
    return 0;
}
