#include <lvgl.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include "screen.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "player_manager.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(clock, LOG_LEVEL_INF);

/* 字体定义 */
LV_FONT_DECLARE(ds_digital);

/* 番茄钟状态 */
typedef enum {
    TOMATO_WORK,
    TOMATO_BREAK,
} tomato_state_t;

/* 数字时钟屏幕私有数据 */
typedef struct {
    struct k_timer update_timer;
    lv_obj_t *left_label;
    lv_obj_t *right_label;
    display_mode_t current_mode;
    bool is_tomato_running;
    int remaining_seconds;
    tomato_state_t tomato_state;
} digital_clock_data_t;

static digital_clock_data_t clock_data = {
    .current_mode = MODE_TIME,
    .is_tomato_running = true,  // 初始暂停
    .remaining_seconds = 25 * 60,
    .tomato_state = TOMATO_WORK  // 初始工作模式
};

void digital_clock_mode_set(int mode)
{
    if (mode >= MODE_TIME && mode < MODE_COUNT) {  
        clock_data.current_mode = mode;
    }
}

int digital_clock_mode_get()
{
    return clock_data.current_mode;
}

void digital_timer_data_set(int mins)
{
    clock_data.remaining_seconds += (mins * 60);

    if(clock_data.remaining_seconds < 0)
        clock_data.remaining_seconds = 0;
    if(clock_data.remaining_seconds >= 100 * 60)
        clock_data.remaining_seconds = 99 * 60;
}

void digital_timer_reset()
{
    clock_data.remaining_seconds = 25 * 60;
    clock_data.is_tomato_running = true;
    clock_data.tomato_state = TOMATO_WORK;
}

void digital_clock_mode_change()
{
    clock_data.current_mode++;
    if (clock_data.current_mode >= MODE_COUNT) {
        clock_data.current_mode = MODE_TIME;
    }
}

/* 新增：切换番茄钟运行/暂停 */
void digital_tomato_toggle()
{
    clock_data.is_tomato_running = !clock_data.is_tomato_running;
}

void tone_toggle_work_handler(struct k_work *work)
{
    app_player_start(TONE_PLAYER, "/lfs/ticking_clock.mp3");
}

K_WORK_DEFINE(tone_toggle_work, tone_toggle_work_handler);

/* 定时器回调 */
extern struct tm utc8_time_start;
extern time_t utc8_seconds_start;
static void update_display(struct k_timer *timer)
{
    static uint16_t tick_count = 0;  // 修正初始值为0
    char left_str[3] = {0}, right_str[3] = {0};
    static int time_m = 0;
    time_m++;

    time_t now = utc8_seconds_start + time_m;
    struct tm *tm = gmtime(&now);

    if (clock_data.current_mode == MODE_TIME) {
        snprintf(left_str, sizeof(left_str), "%02d", tm->tm_hour);
        snprintf(right_str, sizeof(right_str), "%02d", tm->tm_min);
    } else if (clock_data.current_mode == MODE_TIMER) {
        int mins = clock_data.remaining_seconds / 60;
        int secs = clock_data.remaining_seconds % 60;
        snprintf(left_str, sizeof(left_str), "%02d", mins);
        snprintf(right_str, sizeof(right_str), "%02d", secs);

        // 番茄钟逻辑：先显示当前值，然后处理递减/切换
        if (clock_data.is_tomato_running) {
            if (clock_data.remaining_seconds > 0) {
                clock_data.remaining_seconds--;
            } else {
                // 时间到：切换阶段，设置新时间，自动继续运行
                clock_data.tomato_state = (clock_data.tomato_state == TOMATO_WORK) ? TOMATO_BREAK : TOMATO_WORK;
                
                if(clock_data.tomato_state == TOMATO_WORK)
                {
                    clock_data.remaining_seconds = 25 * 60;
                    // app_player_start(TONE_PLAYER, "/lfs/time_up.mp3");
                }
                else
                {
                    clock_data.remaining_seconds = 5 * 60;
                    // app_player_start(TONE_PLAYER, "/lfs/time_up.mp3");
                }
            }
        }

        if(tick_count++ % 7 == 0)
        {
            tick_count = 0;
            k_work_submit(&tone_toggle_work);  // 每秒播放一次提示音
        }
    }

    lv_label_set_text(clock_data.left_label, left_str);
    lv_label_set_text(clock_data.right_label, right_str);

    screen_manager.screens[DISPLAY_LEFT].needs_update = true;
    screen_manager.screens[DISPLAY_RIGHT].needs_update = true;
    k_msgq_put(&display_flush_msgq, NULL, K_NO_WAIT);
}

static int digital_clock_load()
{
    lv_obj_t *screen_left = screen_manager.screens[DISPLAY_LEFT].root;
    lv_obj_t *screen_right = screen_manager.screens[DISPLAY_RIGHT].root;
    
    clock_data.left_label = lv_label_create(screen_left);
    lv_label_set_text(clock_data.left_label, "88");
    lv_obj_set_style_text_font(clock_data.left_label, &ds_digital, LV_PART_MAIN);
    lv_obj_set_style_text_color(clock_data.left_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_center(clock_data.left_label);

    clock_data.right_label = lv_label_create(screen_right);
    lv_label_set_text(clock_data.right_label, "88");
    lv_obj_set_style_text_font(clock_data.right_label, &ds_digital, LV_PART_MAIN);
    lv_obj_set_style_text_color(clock_data.right_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_center(clock_data.right_label);

    screen_manager.screens[DISPLAY_LEFT].needs_update = true;
    screen_manager.screens[DISPLAY_RIGHT].needs_update = true;
    k_msgq_put(&display_flush_msgq, NULL, K_NO_WAIT);

    k_timer_init(&clock_data.update_timer, update_display, NULL);
    k_timer_start(&clock_data.update_timer, K_MSEC(1000), K_MSEC(1000));
    
    return 0;
}

static int digital_clock_update(void)
{
    // 如果需要强制更新，可在此添加逻辑
    // 例如：update_display(NULL);
    return 0;
}

static int digital_clock_unload(void)
{
    k_timer_stop(&clock_data.update_timer);

    if (clock_data.left_label != NULL) {
        lv_obj_del(clock_data.left_label);
        clock_data.left_label = NULL;
    }
    
    if (clock_data.right_label != NULL) {
        lv_obj_del(clock_data.right_label);
        clock_data.right_label = NULL;
    }

    return 0;
}

/* 屏幕接口 */
screen_interface_t digital_clock_screen_interface = {
    .load = digital_clock_load,
    .update = digital_clock_update,
    .unload = digital_clock_unload,
};