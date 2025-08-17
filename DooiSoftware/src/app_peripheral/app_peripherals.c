#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "csk_malloc.h"
#include "ble_connect.h"
#include "lsc_session_request.h"
#include "player_manager.h"
#include "app_peripherals.h"
#include "action_sequences.h"
#include <stdlib.h>
#include "app_chat_session.h"
LOG_MODULE_REGISTER(app_peripherals, LOG_LEVEL_DBG);

#define EXGPIOA_2_NODE_D DT_ALIAS(led0)
static const struct gpio_dt_spec gpio_exa2 = GPIO_DT_SPEC_GET(EXGPIOA_2_NODE_D, gpios);

/* 外部引入*/
extern int s_music_item_cnt;
extern int s_playing_index; 
extern void request_music_url_and_play(int index);

static void handle_global_buttons(button_t button, button_event_t event) 
{
    static bool is_screen_info = false, is_page_select = false;
    if(screen_manager.current_screen == SCREEN_EYES)
    {
        return;
    }
    
    if(event == BUTTON_LONG_PRESS) 
    {
        if(button == BUTTON_LEFT)
        {
            if(is_screen_info)
            {
                screen_set_current(screen_manager.old_screen);
                is_screen_info = false;
            }
            else
            {
                screen_set_current(SCREEN_INFO);
                is_screen_info = true;
            }
        }
        else
        {
            if(is_page_select)
            {
                screen_set_current(screen_manager.old_screen);
                is_page_select = false;
            }
            else
            {
                screen_set_current(SCREEN_PAGE_SELECT);
                is_page_select = true;
            }            
        }
    }

    if(event == BUTTON_TRIPLE_CLICK) 
    {
        if(button == BUTTON_LEFT)
        {
            screen_load_next();
        }
        else
        {
            screen_sub_load();
        }
    }
}

static void clock_screen_handler(button_t button, button_event_t event) 
{
    if(digital_clock_mode_get() == MODE_TIMER)
    {
        switch(event) 
        {
        case BUTTON_SINGLE_CLICK:
            if(button == BUTTON_LEFT)
            {
                digital_timer_data_set(5);
            }
            else
            {
                digital_timer_data_set(-5);
            }
            break;
            
        case BUTTON_DOUBLE_CLICK:
            digital_timer_reset();  
            break;
        default:
            break;
        }
    }
}

static void music_screen_handler(button_t button, button_event_t event) 
{
    switch(event)
    {
    case BUTTON_SINGLE_CLICK:
        if(button == BUTTON_LEFT)
        {
            int volume = 0;
			app_player_get_volume(MUSIC_PLAYER, &volume);
            LOG_INF("current music volume: %d", volume);
			volume -= 10;
			if (volume < 0)
				volume = 0;
			app_player_set_sys_volume(volume);
        }
        else
        {
            int volume = 0;
			app_player_get_volume(MUSIC_PLAYER, &volume);
            LOG_INF("current music volume: %d", volume);
			volume += 10;
			if (volume > 100)
				volume = 100;
			app_player_set_sys_volume(volume);
        }
        break;
    case BUTTON_DOUBLE_CLICK:
        if(button == BUTTON_LEFT)
        {
            if ((s_playing_index + 1) < s_music_item_cnt)
            {
				s_playing_index++;
			}
			request_music_url_and_play(s_playing_index);
        }
        else
        {
            if ((s_playing_index - 1) >= 0) 
            {
				s_playing_index--;
			}
            request_music_url_and_play(s_playing_index);
        }
        break;
    default:
        break;
    }
}

static void camera_screen_handler(button_t button, button_event_t event) 
{
    static bool is_camera_takepic = false;
    switch(event) 
    {
    case BUTTON_SINGLE_CLICK:
        if(is_camera_takepic)
        {
            camera_func_set(CAMERA_SHOW);
            is_camera_takepic = false;
        }
        else
        {
            camera_func_set(CAMERA_TAKE_PIC);
            is_camera_takepic = true;
        }
        break;
        
    case BUTTON_DOUBLE_CLICK:
            camera_func_set(CAMERA_OBJ_REC);
        break;
    default:
        break;
    }
}


static void eyes_screen_handler(button_t button, button_event_t event) 
{
    static uint8_t click_count = 0;
    static button_t click_sequence[5] = {0};  // 扩展到5个，允许更多组合创新
    static uint32_t last_click_time = 0;
    uint32_t current_time = k_uptime_get_32();
    
    switch(event) 
    {
    case BUTTON_SINGLE_CLICK:
        // 重置点击序列（如果超过10秒）
        if (current_time - last_click_time > 10000) {
            click_count = 0;
        }
        
        // 记录点击序列（扩展到最多5次，增加创新组合）
        if (click_count < 5) {
            click_sequence[click_count++] = button;
            last_click_time = current_time;
        }
        
        // 基础眼部聚焦
        if (button == BUTTON_LEFT) {
            action_sequences_enqueue(SEQUENCE_LEFT_EYE_FOCUS);
        } else {
            action_sequences_enqueue(SEQUENCE_RIGHT_EYE_FOCUS);
        }
        
        // 创新：检测时间间隔，如果太快（<500ms），触发惊讶（模拟机器人被“吓到”）
        // if (click_count >= 2 && (current_time - last_click_time) < 500) {
        //     action_sequences_enqueue(SEQUENCE_SURPRISED);
        //     click_count = 0;  // 重置以避免重复
        //     break;
        // }
        
        // 检查组合动作模式（扩展原有3次combo，并添加新模式）
        if (click_count >= 3) {
            // 原有combo：左-右-左 或 右-左-右 触发矫情
            bool is_combo_left_right = (click_sequence[0] == BUTTON_LEFT && 
                                        click_sequence[1] == BUTTON_RIGHT && 
                                        click_sequence[2] == BUTTON_LEFT);
            bool is_combo_right_left = (click_sequence[0] == BUTTON_RIGHT && 
                                        click_sequence[1] == BUTTON_LEFT && 
                                        click_sequence[2] == BUTTON_RIGHT);
            if (is_combo_left_right || is_combo_right_left) {
                action_sequences_enqueue(SEQUENCE_COMBO_ACTION);
            }
            
            // 新创新combo：左右交替3次以上触发跳舞（娱乐模式）
            bool is_alternating = true;
            for (uint8_t i = 1; i < click_count; i++) {
                if (click_sequence[i] == click_sequence[i-1]) {
                    is_alternating = false;
                    break;
                }
            }
            if (is_alternating && click_count >= 3) {
                action_sequences_enqueue(SEQUENCE_DANCE_ROUTINE);
            }
            
            // 新创新：重复同一按钮3次触发困惑或思考（模拟机器人“纠结”）
            bool is_repeated_left = (click_sequence[0] == BUTTON_LEFT && 
                                     click_sequence[1] == BUTTON_LEFT && 
                                     click_sequence[2] == BUTTON_LEFT);
            bool is_repeated_right = (click_sequence[0] == BUTTON_RIGHT && 
                                      click_sequence[1] == BUTTON_RIGHT && 
                                      click_sequence[2] == BUTTON_RIGHT);
            if (is_repeated_left) {
                action_sequences_enqueue(SEQUENCE_CONFUSED);
            } else if (is_repeated_right) {
                action_sequences_enqueue(SEQUENCE_THINKING);
            }
            
            // 如果超过5次，重置并触发巡逻（模拟机器人“无聊了，开始探索”）
            if (click_count >= 5) {
                action_sequences_enqueue(SEQUENCE_PATROL);
                click_count = 0;
            }
        }
        break;
        
    case BUTTON_DOUBLE_CLICK:
        // 根据按钮区分动作（左双击眯眼可爱，右双击问候友好）
        if (button == BUTTON_LEFT) {
            action_sequences_enqueue(SEQUENCE_CUTE);     // 可爱序列，增加趣味
        } else {
            action_sequences_enqueue(SEQUENCE_GREETING);
        }
        break;
        
    case BUTTON_LONG_PRESS:
        // 长按触发情绪序列（左长按生气，右长按开心跳跃，模拟持久情绪）
        if (button == BUTTON_LEFT) {
            action_sequences_enqueue(SEQUENCE_ANGRY);
        } else {
            action_sequences_enqueue(SEQUENCE_HAPPY);
        }
        break;
        
    default:
        break;
    }
}

static void info_screen_handler(button_t button, button_event_t event) 
{
    info_settings_handle_key(button, event);
}

static void page_select_screen_handler(button_t button, button_event_t event) 
{
    screen_select_handle_key(button, event);
}


void app_handle_button(button_t button, button_event_t event) 
{
    // 全局按键处理
    handle_global_buttons(button, event);
    
    // 页面特定处理
    switch (screen_manager.current_screen) 
    {
    case SCREEN_DIGITAL_CLOCK:
        clock_screen_handler(button, event);
        break;
        
    case SCREEN_MUSIC_SPECTRUM:
        music_screen_handler(button, event);
        break;
        
    case SCREEN_EYES:
        eyes_screen_handler(button, event);
        break;
        
    case SCREEN_CAMERA:
        camera_screen_handler(button, event);
        break;
    case SCREEN_INFO:
        info_screen_handler(button, event);
        break;
    case SCREEN_PAGE_SELECT:
        page_select_screen_handler(button, event);
    default:
        break;
    }
}

/* 外设主任务 */
void app_peripherals(void *p1, void *p2, void *p3)
{
    // uint16_t tick = 0, status = 0;
    // gpio_pin_configure_dt(&gpio_exa2, GPIO_OUTPUT_LOW);
    
    soc_adc_init();
    pca9635_init();
    motor_init();
    servo_init();
    led_init();
    app_key_init();

    while (1) 
    {
        // if(tick++ > 500)
        // {
        //     tick = 0;
        //     status = !status;
        //     gpio_pin_set_dt(&gpio_exa2, status);
        // }

        led_process();
        motor_process();
        servo_process();
        action_sequences_process();

        k_msleep(20);
    }
}

static int app_peripheraid_init(void)
{
    k_thread_stack_t *stack;
    k_tid_t tid;
    const int stack_size = 4 * 1024;
    const int thread_prio = 10;

    stack = csk_aligned_alloc(8, stack_size);
    if (stack == NULL) {
        LOG_ERR("app_peripheraid stack malloc failed");
        return -ENOMEM;
    }

    struct k_thread *new_thread = csk_malloc(sizeof(struct k_thread));
    if (new_thread == NULL) {
        csk_free(stack);
        return -ENOMEM;
    }

    tid = k_thread_create(new_thread, stack, stack_size, app_peripherals, 
                         NULL, NULL, NULL, thread_prio, 0, K_NO_WAIT);
    if (tid != new_thread) {
        csk_free(new_thread);
        csk_free(stack);
        LOG_ERR("app_peripheraid thread creation failed");
        return -EFAULT;
    }

    k_thread_name_set(tid, "app_peripheraid");
    return 0;
}

SYS_INIT(app_peripheraid_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);


