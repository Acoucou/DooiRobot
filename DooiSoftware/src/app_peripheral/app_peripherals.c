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


// ====== 配置参数（可按体验调优）======
enum {
    GESTURE_HISTORY_MAX           = 8,     // 历史事件长度
    GESTURE_RESET_MS              = 10000, // 10s 无操作重置
    GESTURE_COOLDOWN_MS           = 350,   // 触发后短冷却，防抖

    // 规则时间窗
    WIN_FAST_TAP_SURPRISED_MS     = 2000,   // 超快两次单击 → 惊讶
    WIN_LR_HAPPY_MS               = 2000,  // 左后右(或右后左) ≤1s → 开心
    WIN_SINGLE_THEN_LONG_BORED_MS = 4000,  // 单击后长按 ≤1.5s → 无聊
    WIN_SINGLE_THEN_DOUBLE_ANGRY_MS = 4000,// 单击后紧接双击 ≤1.5s → 生气
    WIN_ALT_DANCE_MS              = 2000,  // 交替≥3次 ≤2s → 跳舞
    WIN_REPEAT3_MS                = 2000,  // 同侧3次 ≤2s → 困惑/思考
    WIN_PATROL_MS                 = 3000,  // ≥5次 ≤3s → 巡逻
};
// ====== 数据结构 ======
typedef struct {
    button_t        side;
    button_event_t  type;
    uint32_t        ts;
} touch_evt_t;

typedef struct {
    touch_evt_t buf[GESTURE_HISTORY_MAX];
    uint8_t     count;
    uint8_t     focus_type;
    uint32_t    last_ts;
    uint32_t    last_fire_ts;
} gesture_ctx_t;

static gesture_ctx_t g_ctx;

// ====== 工具函数 ======
static inline uint32_t now_ms(void) {
    return k_uptime_get_32();
}

static void history_reset_if_idle(uint32_t ts) {
    if (g_ctx.count == 0) return;
    if (ts - g_ctx.last_ts > GESTURE_RESET_MS) {
        g_ctx.focus_type = !g_ctx.focus_type;
        g_ctx.count = 0;
    }
}

static void history_push(button_t side, button_event_t type, uint32_t ts) 
{
    history_reset_if_idle(ts);
    if (g_ctx.count >= GESTURE_HISTORY_MAX) {
        // 左移丢弃最旧
        memmove(&g_ctx.buf[0], &g_ctx.buf[1], sizeof(touch_evt_t)*(GESTURE_HISTORY_MAX-1));
        g_ctx.count = GESTURE_HISTORY_MAX-1;
    }
    g_ctx.buf[g_ctx.count++] = (touch_evt_t){ .side = side, .type = type, .ts = ts };
    g_ctx.last_ts = ts;
}

static bool within(uint32_t newer, uint32_t older, uint32_t window_ms) {
    return (newer >= older) && (newer - older <= window_ms);
}

static sequence_type_t random_happy(void) {
    uint32_t r = rand() % 3;
    switch (r) {
        case 0: return SEQUENCE_HAPPY;
        case 1: return SEQUENCE_HAPPY1;
        default:return SEQUENCE_HAPPY2;
    }
}

// ====== 规则匹配（按优先级从高到低）======
//
// 优先级设计：
// 1) 单击→双击（愤怒） > 单击→长按（无聊） > LR/RL快速（开心） > 长按（愤怒） > 双击（随机开心）
// 2) 创新：快节奏惊讶、交替跳舞、三连困惑/思考、5+巡逻
// 3) 基础：单击立即看向一边（总是生效）
//
// 命中规则后通常清空历史或部分清空，避免连环误触发。
//
static bool match_single_then_double_angry(uint32_t ts_now) {
    if (g_ctx.count < 2) return false;
    touch_evt_t a = g_ctx.buf[g_ctx.count-2];
    touch_evt_t b = g_ctx.buf[g_ctx.count-1];
    if (a.type == BUTTON_SINGLE_CLICK && b.type == BUTTON_DOUBLE_CLICK &&
        within(b.ts, a.ts, WIN_SINGLE_THEN_DOUBLE_ANGRY_MS)) 
    {
        LOG_INF("match_single_then_double_angry");

        if(b.side == BUTTON_RIGHT)
        {
            action_sequences_enqueue(SEQUENCE_ANGRY_FOCUS_LEFT);
        }
        else
        {
            action_sequences_enqueue(SEQUENCE_ANGRY_FOCUS_RIGHT);
        }
        g_ctx.count = 0;
        g_ctx.last_fire_ts = ts_now;
        return true;
    }
    return false;
}

static bool match_single_then_long_bored(uint32_t ts_now) 
{
    if (g_ctx.count < 2) return false;

    touch_evt_t a = g_ctx.buf[g_ctx.count-2];
    touch_evt_t b = g_ctx.buf[g_ctx.count-1];
    if (a.type == BUTTON_SINGLE_CLICK && b.type == BUTTON_LONG_PRESS &&
        within(b.ts, a.ts, WIN_SINGLE_THEN_LONG_BORED_MS)) 
    {
        LOG_INF("match_single_then_long_bored");
        action_sequences_enqueue(SEQUENCE_ANGRY + (rand() % 2)); // 随机angry1/2;
        g_ctx.count = 0;
        g_ctx.last_fire_ts = ts_now;
        return true;
    }
    return false;
}

static bool match_lr_happy(uint32_t ts_now) {
    if (g_ctx.count < 2) return false;

    touch_evt_t a = g_ctx.buf[g_ctx.count-2];
    touch_evt_t b = g_ctx.buf[g_ctx.count-1];
    if (a.type == BUTTON_SINGLE_CLICK && b.type == BUTTON_SINGLE_CLICK &&
        within(b.ts, a.ts, WIN_LR_HAPPY_MS) &&
        ((a.side == BUTTON_LEFT  && b.side == BUTTON_RIGHT) ||
         (a.side == BUTTON_RIGHT && b.side == BUTTON_LEFT))) 
    {
        LOG_INF("match_lr_happy");

        action_sequences_enqueue(random_happy());
        g_ctx.count = 0;
        g_ctx.last_fire_ts = ts_now;
        return true;
    }
    return false;
}

static bool match_long_angry(uint32_t ts_now) {
    if (g_ctx.count < 1) return false;

    touch_evt_t b = g_ctx.buf[g_ctx.count-1];
    if (b.type == BUTTON_LONG_PRESS) 
    {
        LOG_INF("match_long_angry");

        action_sequences_enqueue(SEQUENCE_ANGRY + (rand() % 2));
        g_ctx.count = 0;
        g_ctx.last_fire_ts = ts_now;
        return true;
    }
    return false;
}

static bool match_double_random_happy(uint32_t ts_now) {
    if (g_ctx.count < 1) return false;

    touch_evt_t b = g_ctx.buf[g_ctx.count-1];
    if (b.type == BUTTON_DOUBLE_CLICK) 
    {
        LOG_INF("match_double_random_happy");

        action_sequences_enqueue(random_happy());
        g_ctx.count = 0;
        g_ctx.last_fire_ts = ts_now;
        return true;
    }
    return false;
}

static bool match_fast_tap_surprised(uint32_t ts_now) {
    // 任意侧两次“单击”且间隔非常短 → 惊讶
    if (g_ctx.count < 2) return false;

    touch_evt_t a = g_ctx.buf[g_ctx.count-2];
    touch_evt_t b = g_ctx.buf[g_ctx.count-1];
    if (a.type == BUTTON_SINGLE_CLICK && b.type == BUTTON_SINGLE_CLICK &&
        within(b.ts, a.ts, WIN_FAST_TAP_SURPRISED_MS)) 
    {
        LOG_INF("match_fast_tap_surprised");

        action_sequences_enqueue(SEQUENCE_FOCUS_BELOW_CENTER + (rand() % 2));
        g_ctx.count = 0;
        g_ctx.last_fire_ts = ts_now;
        return true;
    }
    return false;
}

static bool match_alternating_dance(uint32_t ts_now) {
    // 最近 ≤WIN_ALT_DANCE_MS 内，左右交替≥3次（单击序列即可）
    int n = 0;
    uint32_t first_ts = 0, last_ts = 0;
    // 从末尾往前数连续单击
    for (int i = g_ctx.count-1; i >= 0; --i) {
        if (g_ctx.buf[i].type != BUTTON_SINGLE_CLICK) break;
        if (i < g_ctx.count-1) {
            if (g_ctx.buf[i].side == g_ctx.buf[i+1].side) {
                // 出现同向，中断
                break;
            }
        }
        if (n == 0) first_ts = g_ctx.buf[i].ts;
        last_ts = g_ctx.buf[g_ctx.count-1].ts;
        n++;
        // 保持搜索但不超出时间窗
        if (last_ts - first_ts > WIN_ALT_DANCE_MS) break;
    }
    if (n >= 3 && (last_ts - first_ts) <= WIN_ALT_DANCE_MS) {
        LOG_INF("match_alternating_dance");

        action_sequences_enqueue(SEQUENCE_COMBO_ACTION);
        g_ctx.count = 0;
        g_ctx.last_fire_ts = ts_now;
        return true;
    }
    return false;
}

static bool match_repeat3_confused_or_thinking(uint32_t ts_now) {
    // 最近同侧单击≥3次 且 ≤时间窗
    if (g_ctx.count < 3) return false;

    touch_evt_t b = g_ctx.buf[g_ctx.count-1];
    touch_evt_t a = g_ctx.buf[g_ctx.count-2];
    touch_evt_t c = g_ctx.buf[g_ctx.count-3];
    if (b.type == BUTTON_SINGLE_CLICK && a.type == BUTTON_SINGLE_CLICK && c.type == BUTTON_SINGLE_CLICK &&
        b.side == a.side && a.side == c.side &&
        within(b.ts, c.ts, WIN_REPEAT3_MS)) 
    {
        LOG_INF("match_repeat3_confused_or_thinking");

        action_sequences_enqueue(b.side == BUTTON_LEFT ? SEQUENCE_CONFUSED : SEQUENCE_THINKING);
        g_ctx.count = 0;
        g_ctx.last_fire_ts = ts_now;
        return true;
    }
    return false;
}

static bool match_patrol_over5(uint32_t ts_now) {
    // 最近≤时间窗的事件数≥5（不限类型/侧）→ 巡逻
    // if (g_ctx.count < 5) return false;
    // LOG_INF("match_patrol_over5");

    // uint32_t first = g_ctx.buf[g_ctx.count-5].ts;
    // uint32_t last  = g_ctx.buf[g_ctx.count-1].ts;
    // if (last - first <= WIN_PATROL_MS) {
    //     action_sequences_enqueue(SEQUENCE_PATROL);
    //     g_ctx.count = 0;
    //     g_ctx.last_fire_ts = ts_now;
    //     return true;
    // }
    // return false;
}

// ====== 基础行为：单击立刻看向一边 ======
static void base_focus_on_single(button_t side) {
    if(g_ctx.focus_type)
    {
        action_sequences_enqueue(side == BUTTON_LEFT ? SEQUENCE_FOCUS_LEFT : SEQUENCE_FOCUS_RIGHT);
    }
    else
    {
        action_sequences_enqueue(side == BUTTON_LEFT ? SEQUENCE_FOCUS_TOP_LEFT : SEQUENCE_FOCUS_TOP_RIGHT);
    }
}

// ====== 公共入口：触摸事件处理 ======
void eyes_screen_handler(button_t button, button_event_t event) {
    uint32_t t = now_ms();

    // 全局冷却（避免短时间内多规则重入）
    if (t - g_ctx.last_fire_ts < GESTURE_COOLDOWN_MS) 
    {
        history_push(button, event, t);
        
        if (event == BUTTON_SINGLE_CLICK) 
        {
            base_focus_on_single(button);
        }
        return;
    }

    // 记录历史
    history_push(button, event, t);

    // —— 基础即时反馈：单击立刻看向一边（无条件执行）——
    if (event == BUTTON_SINGLE_CLICK) {
        base_focus_on_single(button); 
    }

    // —— 规则匹配（按优先级）——
    // 单击之后加双击「生气」
    if (match_single_then_double_angry(t)) return;

    // 单击 + 长按 「无聊」
    if (match_single_then_long_bored(t)) return;

    // 左右单击「开心」
    if (match_lr_happy(t)) return;

    // 长按 「生气」
    if (match_long_angry(t)) return;

    // 双击 「随机开心」
    if (match_double_random_happy(t)) return;

    // 超快单击两次 → 惊讶
    if (match_fast_tap_surprised(t)) return;

    // 交替 ≥3 次 → 跳舞（娱乐）
    if (match_alternating_dance(t)) return;

    // 同侧 3 次 → 困惑/思考
    if (match_repeat3_confused_or_thinking(t)) return;

    // ≥5 次 → 巡逻
    if (match_patrol_over5(t)) return;

    // 其它组合可继续在此追加……
}


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


