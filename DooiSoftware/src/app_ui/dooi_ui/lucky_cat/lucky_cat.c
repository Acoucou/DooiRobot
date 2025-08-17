#include <lvgl.h>
#include <zephyr/kernel.h>
#include "screen.h"

typedef struct {
    lv_obj_t * right_obj;
    lv_obj_t * left_obj;
    int servo_tick;
} lucky_cat_t;

static lucky_cat_t lucky_cat;  // 全局实例

LV_IMG_DECLARE(lucky_cat1);

static int lucky_cat_load() {
    lv_obj_t* screen_left = screen_manager.screens[DISPLAY_LEFT].root;
    lv_obj_t* screen_right = screen_manager.screens[DISPLAY_RIGHT].root;

    lucky_cat.left_obj = lv_obj_create(screen_left);
    lv_obj_remove_style_all(lucky_cat.left_obj);
    lv_obj_set_size(lucky_cat.left_obj, 140, 140);
    lv_obj_set_style_radius(lucky_cat.left_obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(lucky_cat.left_obj, lv_color_hex(0xe9dbfc), 0);
    lv_obj_set_style_border_width(lucky_cat.left_obj, 2, 0);                   // 设置边框宽度
    lv_obj_set_style_border_color(lucky_cat.left_obj, lv_color_hex(0xe9dbfc), 0); // 设置边框颜色
    lv_obj_align(lucky_cat.left_obj, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *lable = lv_label_create(lucky_cat.left_obj);
    lv_label_set_text(lable, "lucky cat");      
    lv_obj_center(lable);   

    /* 在右屏创建GIF */
    lucky_cat.right_obj = lv_gif_create(screen_right);
    lv_gif_set_src(lucky_cat.right_obj, &lucky_cat1);
    lv_obj_align(lucky_cat.right_obj, LV_ALIGN_CENTER, 0, 0);

    return 0;
}

/* 页面更新函数 */
static int lucky_cat_update() 
{
    lv_timer_handler();  
    return 0;
}

/* 卸载页面 */
static int lucky_cat_unload() 
{
    if (lucky_cat.left_obj) {
        lv_obj_del(lucky_cat.left_obj);
        lucky_cat.left_obj = NULL;
    }
    
    if (lucky_cat.right_obj) {
        lv_obj_del(lucky_cat.right_obj);
        lucky_cat.right_obj = NULL;
    }
    
    return 0;
}

screen_interface_t lucky_cat_screen_interface = 
{
    .load = lucky_cat_load,
    .update = lucky_cat_update,
    .unload = lucky_cat_unload
};

