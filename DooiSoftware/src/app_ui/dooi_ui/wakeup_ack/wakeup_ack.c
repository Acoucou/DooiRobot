#include <lvgl.h>
#include <zephyr/kernel.h>
#include "screen.h"

typedef struct {
    lv_obj_t * right_obj;
    lv_obj_t * left_obj;
    lv_obj_t * text_lable;
} wakeup_ack_t;

static wakeup_ack_t wakeup_ack;  // 全局实例

/* 字体定义 */
LV_FONT_DECLARE(lv_font_notosans_cs_medium_14);

LV_IMG_DECLARE(doro1);
LV_IMG_DECLARE(siri);

void set_wakeup_ack_text(char * text) 
{
    char safe_text[128];
    strncpy(safe_text, text, 128 - 1);
    safe_text[128 - 1] = '\0';

    k_mutex_lock(&display_mutex, K_FOREVER);

    if(screen_manager.current_screen != SCREEN_WAKEUP_ACK && screen_manager.screen_states[screen_manager.current_screen] == SCREEN_LOADED)
    {
	    k_mutex_unlock(&display_mutex);
        return;
    }
    
    lv_label_set_text(wakeup_ack.text_lable, safe_text);

	k_mutex_unlock(&display_mutex);
}

static int wakeup_ack_load() {
    lv_obj_t* screen_left = screen_manager.screens[DISPLAY_LEFT].root;
    lv_obj_t* screen_right = screen_manager.screens[DISPLAY_RIGHT].root;

    wakeup_ack.left_obj = lv_obj_create(screen_left);
    lv_obj_remove_style_all(wakeup_ack.left_obj);
    lv_obj_set_size(wakeup_ack.left_obj, 200, 200);
    lv_obj_set_style_radius(wakeup_ack.left_obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(wakeup_ack.left_obj, lv_color_hex(0xe9dbfc), 0);
    lv_obj_set_style_border_width(wakeup_ack.left_obj, 2, 0);                   // 设置边框宽度
    lv_obj_set_style_border_color(wakeup_ack.left_obj, lv_color_hex(0xe9dbfc), 0); // 设置边框颜色
    lv_obj_align(wakeup_ack.left_obj, LV_ALIGN_CENTER, 0, 0);

    wakeup_ack.text_lable = lv_label_create(wakeup_ack.left_obj);
    lv_obj_set_width(wakeup_ack.text_lable, 160); // 设置你期望的宽度
    lv_label_set_long_mode(wakeup_ack.text_lable, LV_LABEL_LONG_WRAP);  // 自动换行
    lv_obj_set_style_text_font(wakeup_ack.text_lable, &lv_font_notosans_cs_medium_14, LV_PART_MAIN);
    lv_label_set_text(wakeup_ack.text_lable, "");
    lv_obj_align(wakeup_ack.text_lable, LV_ALIGN_CENTER, 0, 0);


    /* 在右屏创建GIF */
    wakeup_ack.right_obj = lv_gif_create(screen_right);
    lv_gif_set_src(wakeup_ack.right_obj, &doro1);
    lv_obj_align(wakeup_ack.right_obj, LV_ALIGN_CENTER, 0, 0);

    return 0;
}

/* 页面更新函数 */
static int wakeup_ack_update() 
{
    screen_manager.screens[DISPLAY_LEFT].needs_update = true;
    screen_manager.screens[DISPLAY_RIGHT].needs_update = true;
    k_msgq_put(&display_flush_msgq, NULL, K_NO_WAIT);
    
    return 0;
}

/* 卸载页面 */
static int wakeup_ack_unload() 
{
    if (wakeup_ack.left_obj) {
        lv_obj_del(wakeup_ack.left_obj);
        wakeup_ack.left_obj = NULL;
    }
    
    if (wakeup_ack.right_obj) {
        lv_obj_del(wakeup_ack.right_obj);
        wakeup_ack.right_obj = NULL;
    }
    
    return 0;
}

screen_interface_t wakeup_ack_screen_interface = 
{
    .load = wakeup_ack_load,
    .update = wakeup_ack_update,
    .unload = wakeup_ack_unload
};

