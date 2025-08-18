#include <lvgl.h>
#include <zephyr/kernel.h>
#include "screen.h"
#include "soc_adc.h"
#include "app_chat_session.h"
#include "ble_connect.h"
#include "player_manager.h"
#include <zephyr/net/socket.h>

#define VERSION_INFO_X "1.0"

typedef struct {
    lv_obj_t *left_container;
    lv_obj_t *label_soc;  

    lv_obj_t *right_container;
    lv_obj_t *switch_ble;
    lv_obj_t *label_ble;
    lv_obj_t *switch_conv;
    lv_obj_t *label_conv;

    bool ble_enabled;
    bool continuous_mode;
    uint8_t focus_item;  // 当前聚焦的设置项 (0:蓝牙, 1:连续对话)
} info_settings_t;

static info_settings_t info_settings;  // 全局实例

static void settings_work_handler(struct k_work *work);
K_WORK_DEFINE(settings_work, settings_work_handler);

/* 动作类型 */
typedef enum {
    ACTION_NONE,
    ACTION_TOGGLE_BLE,
    ACTION_TOGGLE_CONT_MODE
} action_t;

static volatile action_t pending_action = ACTION_NONE;

/* Work Handler：在系统线程中执行 */
static void settings_work_handler(struct k_work *work)
{
    action_t action = pending_action;
    pending_action = ACTION_NONE;  // 重置

    switch (action) {
    case ACTION_TOGGLE_BLE:
        info_settings.ble_enabled = !info_settings.ble_enabled;
        printk("Switch BLE: %d\n", info_settings.ble_enabled);

        if(info_settings.ble_enabled)
        {
            ble_start_adv();
            lv_obj_add_state(info_settings.switch_ble, LV_STATE_CHECKED);
            lv_obj_clear_state(info_settings.switch_ble, LV_STATE_DISABLED); // 清除禁用状态
        }
        else
        {
            ble_stop_adv();
            lv_obj_add_state(info_settings.switch_ble, LV_STATE_DISABLED); // 添加禁用状态
            lv_obj_clear_state(info_settings.switch_ble, LV_STATE_CHECKED); // 清除选中状态
        }

        break;

    case ACTION_TOGGLE_CONT_MODE:
        info_settings.continuous_mode = !info_settings.continuous_mode;
        printk("Switch Continuous Mode: %d\n", info_settings.continuous_mode);

        if (info_settings.continuous_mode) {
            app_player_start(TONE_PLAYER, "/lfs/102_conversation_open.mp3");
            app_chat_session_set_interactive_mode(SESSION_VOICE_INTER_MODE_CONTINUE);

            lv_obj_add_state(info_settings.switch_ble, LV_STATE_CHECKED);
            lv_obj_clear_state(info_settings.switch_ble, LV_STATE_DISABLED); // 清除禁用状态
        } else {
            app_player_start(TONE_PLAYER, "/lfs/103_conversation_close.mp3");
            app_chat_session_set_interactive_mode(SESSION_VOICE_INTER_MODE_ONESHOT);

            lv_obj_add_state(info_settings.switch_ble, LV_STATE_DISABLED); // 添加禁用状态
            lv_obj_clear_state(info_settings.switch_ble, LV_STATE_CHECKED); // 清除选中状态
        }
        break;

    default:
        break;
    }
}

/* 按键事件处理（中断上下文） */
void info_settings_handle_key(button_t button, button_event_t event)
{
    if (button == BUTTON_LEFT) {
        /* 左键：切换焦点，仅修改UI焦点，可以在ISR中执行 */
        info_settings.focus_item = (info_settings.focus_item + 1) % 2;
        lv_obj_clear_state(info_settings.switch_ble, LV_STATE_FOCUSED);
        lv_obj_clear_state(info_settings.switch_conv, LV_STATE_FOCUSED);

        if (info_settings.focus_item == 0) {
            lv_obj_add_state(info_settings.switch_ble, LV_STATE_FOCUSED);
            lv_obj_set_style_text_color(info_settings.switch_ble, lv_color_hex(0x00AEEF), 0);
            lv_obj_set_style_text_color(info_settings.switch_conv, lv_color_hex(0x000000), 0);
        } else {
            lv_obj_add_state(info_settings.switch_conv, LV_STATE_FOCUSED);
            lv_obj_set_style_text_color(info_settings.switch_conv, lv_color_hex(0x00AEEF), 0);
            lv_obj_set_style_text_color(info_settings.switch_ble, lv_color_hex(0x000000), 0);
        }
    } else {
        /* 右键：触发异步动作 */
        if (info_settings.focus_item == 0) {
            pending_action = ACTION_TOGGLE_BLE;
        } else {
            pending_action = ACTION_TOGGLE_CONT_MODE;
        }
        k_work_submit(&settings_work);  // 提交任务到工作队列
    }
}

static int info_settings_load() {
    uint8_t soc_percent = 0;
    soc_adc_enable();

    lv_obj_t* screen_left = screen_manager.screens[DISPLAY_LEFT].root;
    lv_obj_t* screen_right = screen_manager.screens[DISPLAY_RIGHT].root;

    // 初始化默认状态
    info_settings.ble_enabled = false;
    info_settings.continuous_mode = false;
    info_settings.focus_item = 0;

    /*********************
     * 左侧屏幕: 基本信息 *
     *********************/
    info_settings.left_container = lv_obj_create(screen_left);
    lv_obj_set_size(info_settings.left_container, 240, 240);
    lv_obj_align(info_settings.left_container, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_flex_flow(info_settings.left_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(info_settings.left_container, 
                         LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 名称
    lv_obj_t *name = lv_label_create(info_settings.left_container);
    lv_label_set_text_fmt(name, "DooiRobot");

    // 作者信息
    lv_obj_t *label_author = lv_label_create(info_settings.left_container);
    lv_label_set_text_fmt(label_author, "coucou & matchstick");

    // 版本信息
    lv_obj_t *label_version = lv_label_create(info_settings.left_container);
    lv_label_set_text_fmt(label_version, "version: %s", VERSION_INFO_X);

    // IP地址
    lv_obj_t *label_ip = lv_label_create(info_settings.left_container);
    /* 打印服务端IP */
    struct net_if *iface = net_if_get_default();
    char buf[NET_IPV4_ADDR_LEN];
    lv_label_set_text_fmt(label_ip, "IP: %s", net_addr_ntop(AF_INET, &iface->config.dhcpv4.requested_ip, buf, sizeof(buf)));

    // SOC状态
    info_settings.label_soc = lv_label_create(info_settings.left_container);
    soc_adc_read_soc(&soc_percent);
    lv_label_set_text_fmt(info_settings.label_soc, "soc: %d%%", soc_percent);

    /*********************
     * 右侧屏幕: 设置选项 *
     *********************/
    info_settings.right_container = lv_obj_create(screen_right);
    lv_obj_set_size(info_settings.right_container, 240, 240);
    lv_obj_align(info_settings.right_container, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_flex_flow(info_settings.right_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(info_settings.right_container, 
                         LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 蓝牙设置
    lv_obj_t *ble_container = lv_obj_create(info_settings.right_container);
    // lv_obj_remove_style_all(ble_container);
    lv_obj_set_size(ble_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(ble_container, LV_FLEX_FLOW_ROW);
    
    info_settings.label_ble = lv_label_create(ble_container);
    lv_label_set_text(info_settings.label_ble, "bluetooth control");
    
    info_settings.switch_ble = lv_switch_create(ble_container);
    lv_obj_add_state(info_settings.switch_ble, LV_STATE_FOCUSED);  // 默认聚焦
    lv_obj_set_size(info_settings.switch_ble, 40, 20);

    // 连续对话设置
    lv_obj_t *conv_container = lv_obj_create(info_settings.right_container);
    // lv_obj_remove_style_all(conv_container);
    lv_obj_set_size(conv_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(conv_container, LV_FLEX_FLOW_ROW);
    
    
    info_settings.label_conv = lv_label_create(conv_container);
    lv_label_set_text(info_settings.label_conv, "dialogue mode");
    
    info_settings.switch_conv = lv_switch_create(conv_container);
    lv_obj_set_size(info_settings.switch_conv, 40, 20);

    return 0;
}

/* 页面更新函数 */
static int info_settings_update() 
{
    static uint8_t tick = 0;

    if(tick++ > 100)
    {
        uint8_t soc_percent = 0;
        tick = 0;
        soc_adc_read_soc(&soc_percent);
        
        lv_label_set_text_fmt(info_settings.label_soc, "soc: %d%%", soc_percent);
    }

    screen_manager.screens[DISPLAY_LEFT].needs_update = true;
    screen_manager.screens[DISPLAY_RIGHT].needs_update = true;
    k_msgq_put(&display_flush_msgq, NULL, K_NO_WAIT);

    return 0;
}

/* 卸载页面 */
static int info_settings_unload() 
{
    soc_adc_disable();

    if (info_settings.left_container) {
        lv_obj_del(info_settings.left_container);
        info_settings.left_container = NULL;
    }
    
    if (info_settings.right_container) {
        lv_obj_del(info_settings.right_container);
        info_settings.right_container = NULL;
    }
    
    return 0;
}

screen_interface_t info_settings_screen_interface = 
{
    .load = info_settings_load,
    .update = info_settings_update,
    .unload = info_settings_unload
};
