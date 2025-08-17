#include <lvgl.h>
#include "screen.h"

/* 页面名称映射 */
static const char* screen_names[] = {
    "Clock",
    "Music",
    "Eyes",
    "Camera",
    "LuckyCat",
};

#define SCREEN_NAME_COUNT (sizeof(screen_names) / sizeof(screen_names[0]))

/* 页面选择数据结构 */
typedef struct {
    lv_obj_t *left_container;    // 左侧容器
    lv_obj_t *right_container;   // 右侧容器
    lv_obj_t **screen_btns;      // 页面按钮数组
    lv_obj_t *info_label;        // 右侧信息标签
    uint8_t focus_index;         // 当前聚焦的页面索引
} screen_select_t;

static screen_select_t screen_select;  // 全局实例

/* 按键处理函数 */
void screen_select_handle_key(button_t button, button_event_t event)
{
    if(event == BUTTON_LONG_PRESS)
    {
        return;
    }
    
    if(button == BUTTON_LEFT)
    {
        // 下移焦点
        if (screen_select.focus_index < SCREEN_NAME_COUNT - 1) 
        {
            screen_select.focus_index++;
            lv_obj_clear_state(screen_select.screen_btns[screen_select.focus_index - 1], 
                              LV_STATE_FOCUSED);
            lv_obj_add_state(screen_select.screen_btns[screen_select.focus_index], 
                           LV_STATE_FOCUSED);
        }
        else
        {
            screen_select.focus_index = 0;
            lv_obj_clear_state(screen_select.screen_btns[screen_select.focus_index - 1], 
                              LV_STATE_FOCUSED);
            lv_obj_add_state(screen_select.screen_btns[screen_select.focus_index], 
                           LV_STATE_FOCUSED);
        }
    }    
    else
    {
        switch (screen_select.focus_index)
        {
        case 0:
            screen_set_current(SCREEN_DIGITAL_CLOCK);
            break;
        case 1:
            screen_set_current(SCREEN_MUSIC_SPECTRUM);
            break;
        case 2:
            screen_set_current(SCREEN_EYES);
            break;
        case 3:
            screen_set_current(SCREEN_CAMERA);
            break;
        case 4:
            screen_set_current(SCREEN_LUCKY_CAT);
            break;
        default:
            screen_set_current(SCREEN_EYES);
            break;
        }
    }

    lv_label_set_text_fmt(screen_select.info_label, "%s", screen_names[screen_select.focus_index]);
}

/* 按钮事件回调 */
static void screen_btn_event_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    uint32_t idx = (uint32_t)lv_event_get_user_data(e);
    
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        screen_select.focus_index = idx;
        for (int i = 0; i < SCREEN_NAME_COUNT; i++) {
            lv_obj_clear_state(screen_select.screen_btns[i], LV_STATE_FOCUSED);
        }
        lv_obj_add_state(btn, LV_STATE_FOCUSED);
        screen_set_current(idx + 1);
    }
}

/* 页面加载函数 */
static int screen_select_load()
{
    lv_obj_t* screen_left = screen_manager.screens[DISPLAY_LEFT].root;
    lv_obj_t* screen_right = screen_manager.screens[DISPLAY_RIGHT].root;

    // 初始化状态
    screen_select.focus_index = 0;
    screen_select.screen_btns = lv_mem_alloc(sizeof(lv_obj_t*) * SCREEN_NAME_COUNT);
    
    /*********************
     * 左侧屏幕: 页面选择列表 *
     *********************/
    screen_select.left_container = lv_obj_create(screen_left);
    lv_obj_remove_style_all(screen_select.left_container);
    lv_obj_set_size(screen_select.left_container, 240, 240);
    lv_obj_align(screen_select.left_container, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_flex_flow(screen_select.left_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(screen_select.left_container, 
                         LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(screen_select.left_container, 10, 0);
    
    // 创建页面选择按钮
    for (int i = 0; i < SCREEN_NAME_COUNT; i++) {
        screen_select.screen_btns[i] = lv_btn_create(screen_select.left_container);
        lv_obj_set_width(screen_select.screen_btns[i], 120);
        lv_obj_add_event_cb(screen_select.screen_btns[i], screen_btn_event_cb, 
                          LV_EVENT_CLICKED, (void*)(intptr_t)i);
        
        lv_obj_t *label = lv_label_create(screen_select.screen_btns[i]);
        lv_label_set_text(label, screen_names[i]);
        lv_obj_center(label);
    }
    lv_obj_add_state(screen_select.screen_btns[0], LV_STATE_FOCUSED);
    
    /*********************
     * 右侧屏幕: 选择信息 *
     *********************/
    screen_select.right_container = lv_obj_create(screen_right);
    lv_obj_remove_style_all(screen_select.right_container);
    lv_obj_set_size(screen_select.right_container, 140, 140);
    lv_obj_set_style_radius(screen_select.right_container, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(screen_select.right_container, lv_color_hex(0xe9dbfc), 0);
    lv_obj_set_style_border_width(screen_select.right_container, 2, 0);                   // 设置边框宽度
    lv_obj_set_style_border_color(screen_select.right_container, lv_color_hex(0xe9dbfc), 0); // 设置边框颜色
    lv_obj_align(screen_select.right_container, LV_ALIGN_CENTER, 0, 0);

    // 创建信息标签
    screen_select.info_label = lv_label_create(screen_select.right_container);
    lv_label_set_text_fmt(screen_select.info_label, "%s", screen_names[screen_select.focus_index]);
    lv_obj_center(screen_select.info_label); 

    return 0;
}

/* 页面更新函数 */
static int screen_select_update()
{
    screen_manager.screens[DISPLAY_LEFT].needs_update = true;
    screen_manager.screens[DISPLAY_RIGHT].needs_update = true;
    k_msgq_put(&display_flush_msgq, NULL, K_NO_WAIT);

    return 0;
}

/* 卸载页面 */
static int screen_select_unload()
{
    if (screen_select.left_container) {
        lv_obj_del(screen_select.left_container);
        screen_select.left_container = NULL;
    }
    
    if (screen_select.right_container) {
        lv_obj_del(screen_select.right_container);
        screen_select.right_container = NULL;
    }
    
    if (screen_select.screen_btns) {
        lv_mem_free(screen_select.screen_btns);
        screen_select.screen_btns = NULL;
    }
    
    return 0;
}

/* 页面接口 */
screen_interface_t screen_select_screen_interface = 
{
    .load = screen_select_load,
    .update = screen_select_update,
    .unload = screen_select_unload
};
