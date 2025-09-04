#ifndef __SCREEN_H__
#define __SCREEN_H__

#include <zephyr/kernel.h>
#include <stdint.h>
#include <lvgl.h>
#include "key.h"
#include "dooi_ui.h"

/* 显示设备枚举 */
typedef enum {
    DISPLAY_RIGHT,
    DISPLAY_LEFT,
    DISPLAY_COUNT
} display_t;

typedef enum{
    DIS_SAME = 0,    // 同线
    DIS_MIRROR = 4,  // 镜像，值不可修改
    DIS_SINGLE = 5,
    DIS_COUNT,
}disp_mode_t;

/* 枚举定义不同的GIF动画 */
typedef enum {
    ANIMATION_CLOSE_EYES_SLOW = 0,
    ANIMATION_CLOSE_EYES_QUICK,
    ANIMATION_EXCITED,
    ANIMATION_FEAR,
    ANIMATION_SAD,
    ANIMATION_DISDAIN,
    ANIMATION_LEFT,
    ANIMATION_RIGHT,
    ANIMATION_ANGRY,
    ANIMATION_COUNT 
} gif_animation_type_t;

/* 显示模式定义 */
typedef enum {
    MODE_TIME,  // 常规时间模式
    MODE_TIMER, // 番茄计时模式
    MODE_COUNT,
    MODE_STATS  // 统计模式
} display_mode_t;

typedef enum{
    CAMERA_SHOW,
    CAMERA_OBJ_REC,
    CAMERA_TAKE_PIC,
    CAMERA_FUNC_COUNT,
    CAMERA_OBJ_REC_ING,
    CAMERA_QRCODE,
    CAMERA_QRCODE_ING,
}camera_func_t;

/* 屏幕类型枚举 */
typedef enum {
    SCREEN_NULL,
    SCREEN_DIGITAL_CLOCK,
    SCREEN_EMOJI_GIF,
    SCREEN_MUSIC_SPECTRUM,
    SCREEN_EYES,
    SCREEN_CAMERA,
    SCREEN_LUCKY_CAT,
    SCREEN_INFO,
    SCREEN_PAGE_SELECT,
    SCREEN_WAKEUP_ACK,
    SCREEN_COUNT
} screen_type_t;

/* 屏幕接口 */
typedef struct {
    int (*load)(void);    /* 加载屏幕内容 */
    int (*update)(void);  /* 更新屏幕内容 */
    int (*unload)(void);  /* 卸载屏幕内容 */
} screen_interface_t;

typedef enum {
    SCREEN_UNLOADED,
    SCREEN_LOADED
} screen_state_t;

/* 屏幕实例 */
typedef struct {
    lv_obj_t *root;         // 根对象
    bool needs_update;      // 需要更新标志
    screen_interface_t *interface; // 屏幕接口
} screen_instance_t;

/* 屏幕管理器 */
typedef struct {
    display_t current_display;      // 当前激活的显示设备
    screen_state_t screen_states[SCREEN_COUNT]; // 每个屏幕的加载状态
    screen_type_t current_screen;    // 当前屏幕类型
    screen_type_t old_screen;        // 上一个屏幕类型
    screen_type_t next_screen;       // 下一个屏幕
    screen_instance_t screens[DISPLAY_COUNT]; // 每个显示设备的屏幕实例
    int tick;
} screen_manager_t;

/* 全局变量声明 */
extern struct k_msgq display_flush_msgq;
extern struct k_mutex display_mutex;

extern screen_manager_t screen_manager;

int digital_clock_mode_get();
void digital_clock_mode_change();
void digital_timer_data_set(int mins);
void digital_timer_reset();

void emoji_gif_change();
void eyes_change();
void camera_func_change();
void screen_sub_load();
void screen_load_next(void);

void info_settings_handle_key(button_t button, button_event_t event);
void screen_select_handle_key(button_t button, button_event_t event);

/* 函数声明 */
void screen_disp_mode_set(uint8_t mode);
void screen_init(void);
void screen_clear();
int screen_set_brightness(uint8_t pwm_duty);
void screen_switch_display(display_t display);
void screen_set_current(screen_type_t screen_type);
void screen_load(screen_type_t screen_type);
void screen_update(void);

void digital_clock_mode_set(int mode);
void eye_emotion_selete(int eye);
void switch_gif(int animation);

void camera_func_set(camera_func_t func);

#endif /* __SCREEN_H__ */
