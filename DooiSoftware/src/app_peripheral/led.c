#include "led.h"
#include "pca9635.h"
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <string.h>

LOG_MODULE_REGISTER(led, LOG_LEVEL_INF);

/* 硬件通道定义（根据实际硬件修改） */
#define LED1_CHANNEL_R  1
#define LED1_CHANNEL_G  0
#define LED1_CHANNEL_B  2

#define LED2_CHANNEL_R  4 
#define LED2_CHANNEL_G  3
#define LED2_CHANNEL_B  5

/* 预计算表配置 */
#define BREATH_TABLE_SIZE   64
#define RAINBOW_TABLE_SIZE  360
#define UPDATE_INTERVAL     20  // 20ms刷新周期

/* 效果状态机 */
typedef enum {
    EFFECT_OFF,
    EFFECT_SOLID,
    EFFECT_BREATHING,
    EFFECT_RAINBOW,
    EFFECT_BLINK
} effect_state_t;

typedef struct {
    effect_state_t state;
    uint32_t colors[2];      // [0]-left, [1]-right
    uint16_t period;         // 效果周期(ms)
    uint16_t counter;
    uint8_t  brightness;
    bool     update_needed;
    bool     symmetric;
} led_effect_t;

static led_effect_t m_effect;
static struct k_timer m_timer;

/* 修正的呼吸表（完整正弦周期） */
static const uint8_t breath_table[BREATH_TABLE_SIZE] = {
    128,140,152,164,176,187,198,208,217,225,232,238,243,247,250,251,
    252,251,250,247,243,238,232,225,217,208,198,187,176,164,152,140,
    128,115,103,91,79,68,57,47,38,30,23,17,12,8,5,4,
    3,4,5,8,12,17,23,30,38,47,57,68,79,91,103,115
};

/* 自动生成的彩虹表 (HSV色相环 GRB格式) */
const uint32_t rainbow_table[360] = {
    /* 000° */ 
    0x00FF00, 0x04FF00, 0x08FF00, 0x0DFF00, 0x11FF00, 0x15FF00, 0x19FF00, 0x1EFF00, 0x22FF00, 0x26FF00, 
    0x2AFF00, 0x2FFF00, 0x33FF00, 0x37FF00, 0x3CFF00, 0x40FF00, 0x44FF00, 0x48FF00, 0x4DFF00, 0x51FF00, 
    0x55FF00, 0x59FF00, 0x5EFF00, 0x62FF00, 0x66FF00, 0x6AFF00, 0x6EFF00, 0x73FF00, 0x77FF00, 0x7BFF00, 
    0x80FF00, 0x84FF00, 0x88FF00, 0x8CFF00, 0x90FF00, 0x95FF00, 0x99FF00, 0x9DFF00, 0xA2FF00, 0xA6FF00, 
    0xAAFF00, 0xAEFF00, 0xB2FF00, 0xB7FF00, 0xBBFF00, 0xBFFF00, 0xC3FF00, 0xC8FF00, 0xCCFF00, 0xD0FF00, 
    0xD4FF00, 0xD9FF00, 0xDDFF00, 0xE1FF00, 0xE5FF00, 0xEAFF00, 0xEEFF00, 0xF2FF00, 0xF7FF00, 0xFBFF00, 
    /* 060° */ 
    0xFFFF00, 0xFFFB00, 0xFFF700, 0xFFF200, 0xFFEE00, 0xFFEA00, 0xFFE600, 0xFFE100, 0xFFDD00, 0xFFD900,
    0xFFD400, 0xFFD000, 0xFFCC00, 0xFFC800, 0xFFC300, 0xFFBF00, 0xFFBB00, 0xFFB700, 0xFFB200, 0xFFAE00,
    0xFFAA00, 0xFFA600, 0xFFA200, 0xFF9D00, 0xFF9900, 0xFF9500, 0xFF9000, 0xFF8C00, 0xFF8800, 0xFF8400,
    0xFF8000, 0xFF7B00, 0xFF7700, 0xFF7300, 0xFF6E00, 0xFF6A00, 0xFF6600, 0xFF6200, 0xFF5E00, 0xFF5900,
    0xFF5500, 0xFF5100, 0xFF4D00, 0xFF4800, 0xFF4400, 0xFF4000, 0xFF3C00, 0xFF3700, 0xFF3300, 0xFF2F00,
    0xFF2A00, 0xFF2600, 0xFF2200, 0xFF1E00, 0xFF1A00, 0xFF1500, 0xFF1100, 0xFF0D00, 0xFF0800, 0xFF0400,
    /* 120° */ 
    0xFF0000, 0xFF0004, 0xFF0008, 0xFF000D, 0xFF0011, 0xFF0015, 0xFF0019, 0xFF001E, 0xFF0022, 0xFF0026,
    0xFF002A, 0xFF002F, 0xFF0033, 0xFF0037, 0xFF003C, 0xFF0040, 0xFF0044, 0xFF0048, 0xFF004D, 0xFF0051,
    0xFF0055, 0xFF0059, 0xFF005E, 0xFF0062, 0xFF0066, 0xFF006A, 0xFF006F, 0xFF0073, 0xFF0077, 0xFF007B,
    0xFF0080, 0xFF0084, 0xFF0088, 0xFF008C, 0xFF0090, 0xFF0095, 0xFF0099, 0xFF009D, 0xFF00A2, 0xFF00A6,
    0xFF00AA, 0xFF00AE, 0xFF00B3, 0xFF00B7, 0xFF00BB, 0xFF00BF, 0xFF00C3, 0xFF00C8, 0xFF00CC, 0xFF00D0,
    0xFF00D4, 0xFF00D9, 0xFF00DD, 0xFF00E1, 0xFF00E5, 0xFF00EA, 0xFF00EE, 0xFF00F2, 0xFF00F7, 0xFF00FB,
    /* 180° */ 
    0xFF00FF, 0xFB00FF, 0xF700FF, 0xF200FF, 0xEE00FF, 0xEA00FF, 0xE500FF, 0xE100FF, 0xDD00FF, 0xD900FF,
    0xD400FF, 0xD000FF, 0xCC00FF, 0xC800FF, 0xC300FF, 0xBF00FF, 0xBB00FF, 0xB700FF, 0xB200FF, 0xAE00FF,
    0xAA00FF, 0xA600FF, 0xA200FF, 0x9D00FF, 0x9900FF, 0x9500FF, 0x9100FF, 0x8C00FF, 0x8800FF, 0x8400FF,
    0x8000FF, 0x7B00FF, 0x7700FF, 0x7300FF, 0x6F00FF, 0x6A00FF, 0x6600FF, 0x6200FF, 0x5E00FF, 0x5900FF,
    0x5500FF, 0x5100FF, 0x4C00FF, 0x4800FF, 0x4400FF, 0x4000FF, 0x3C00FF, 0x3700FF, 0x3300FF, 0x2F00FF,
    0x2B00FF, 0x2600FF, 0x2200FF, 0x1E00FF, 0x1900FF, 0x1500FF, 0x1100FF, 0x0D00FF, 0x0800FF, 0x0400FF,
    /* 240° */ 
    0x0000FF, 0x0004FF, 0x0008FF, 0x000DFF, 0x0011FF, 0x0015FF, 0x0019FF, 0x001EFF, 0x0022FF, 0x0026FF,
    0x002AFF, 0x002FFF, 0x0033FF, 0x0037FF, 0x003CFF, 0x0040FF, 0x0044FF, 0x0048FF, 0x004CFF, 0x0051FF,
    0x0055FF, 0x0059FF, 0x005DFF, 0x0062FF, 0x0066FF, 0x006AFF, 0x006FFF, 0x0073FF, 0x0077FF, 0x007BFF,
    0x0080FF, 0x0084FF, 0x0088FF, 0x008CFF, 0x0090FF, 0x0095FF, 0x0099FF, 0x009DFF, 0x00A2FF, 0x00A6FF,
    0x00AAFF, 0x00AEFF, 0x00B3FF, 0x00B7FF, 0x00BBFF, 0x00BFFF, 0x00C3FF, 0x00C8FF, 0x00CCFF, 0x00D0FF,
    0x00D5FF, 0x00D9FF, 0x00DDFF, 0x00E1FF, 0x00E6FF, 0x00EAFF, 0x00EEFF, 0x00F2FF, 0x00F7FF, 0x00FBFF,
    /* 300° */ 
    0x00FFFF, 0x00FFFB, 0x00FFF7, 0x00FFF2, 0x00FFEE, 0x00FFEA, 0x00FFE6, 0x00FFE1, 0x00FFDD, 0x00FFD9,
    0x00FFD4, 0x00FFD0, 0x00FFCC, 0x00FFC8, 0x00FFC3, 0x00FFBF, 0x00FFBB, 0x00FFB7, 0x00FFB3, 0x00FFAE,
    0x00FFAA, 0x00FFA6, 0x00FFA1, 0x00FF9D, 0x00FF99, 0x00FF95, 0x00FF90, 0x00FF8C, 0x00FF88, 0x00FF84,
    0x00FF80, 0x00FF7B, 0x00FF77, 0x00FF73, 0x00FF6F, 0x00FF6A, 0x00FF66, 0x00FF62, 0x00FF5E, 0x00FF59,
    0x00FF55, 0x00FF51, 0x00FF4D, 0x00FF48, 0x00FF44, 0x00FF40, 0x00FF3C, 0x00FF37, 0x00FF33, 0x00FF2F,
    0x00FF2B, 0x00FF26, 0x00FF22, 0x00FF1E, 0x00FF1A, 0x00FF15, 0x00FF11, 0x00FF0D, 0x00FF08, 0x00FF04,
};


/* 定时器回调 */
static void timer_handler(struct k_timer *timer) {
    m_effect.update_needed = true;
}

/* 颜色分量提取（适配GRB顺序） */
static void decode_color(uint32_t color, uint8_t *ch_r, uint8_t *ch_g, uint8_t *ch_b) {
    *ch_g = (color >> 16) & 0xFF;  // 红色在高位
    *ch_r = (color >> 8)  & 0xFF;  // 绿色在中位
    *ch_b = color & 0xFF;          // 蓝色在低位
}

/* 立即更新PWM输出 */
static void apply_color(uint32_t left_color, uint32_t right_color) {
    // uint8_t r, g, b;
    static uint8_t buf[6] = {0};

    decode_color(left_color, buf+0, buf+1, buf+2);
    decode_color(right_color, buf+3, buf+4, buf+5);
    
    
    // const uint8_t *ch = (side == 0) ? 
    //     (uint8_t[]){LED1_CHANNEL_R, LED1_CHANNEL_G, LED1_CHANNEL_B} :
    //     (uint8_t[]){LED2_CHANNEL_R, LED2_CHANNEL_G, LED2_CHANNEL_B};
    

    pca9635_set_pwms(0xA0, buf, 6);

    // pca9635_set_pwm(ch[0], r);
    // pca9635_set_pwm(ch[1], g);
    // pca9635_set_pwm(ch[2], b);
}

/* 效果更新处理 */
static void effect_update() {
    if (!m_effect.update_needed) return;
    
    m_effect.counter = (m_effect.counter + 1) % (m_effect.period / UPDATE_INTERVAL);
    
    switch (m_effect.state) {
    case EFFECT_SOLID:
        apply_color(m_effect.colors[0], m_effect.colors[1]);
        break;
        
    case EFFECT_BREATHING: {
        uint8_t idx = (m_effect.counter * BREATH_TABLE_SIZE * UPDATE_INTERVAL) / 
                     m_effect.period % BREATH_TABLE_SIZE;
        uint8_t ratio = breath_table[idx];
        uint8_t r, g, b;
        decode_color(m_effect.colors[0], &r, &g, &b);
        
        uint32_t mixed = ((r * ratio / 255) << 16) | 
                        ((g * ratio / 255) << 8) | 
                        (b * ratio / 255);

        if (m_effect.symmetric) {
            apply_color(mixed, mixed);
        } else {
            apply_color(mixed, 0);
        }
        break;
    }
    
    case EFFECT_RAINBOW: {
        uint16_t hue = (m_effect.counter * 360 * UPDATE_INTERVAL) / m_effect.period % 360;
        
        if (m_effect.symmetric) {
            apply_color(rainbow_table[hue], rainbow_table[hue]);
        } else {
            apply_color(rainbow_table[hue], rainbow_table[(hue + 180) % 360]); 
        }
        break;
    }
    
    case EFFECT_BLINK:
        if ((m_effect.counter / (m_effect.period/(2*UPDATE_INTERVAL))) % 2) {
            apply_color(0, 0);
        } else {
            if (m_effect.symmetric) {
                apply_color(m_effect.colors[0], m_effect.colors[0]);
            } else {
                apply_color(m_effect.colors[0], 0);
            }
        }
        break;
        
    default:
        break;
    }
    
    m_effect.update_needed = false;
}

/* 公共接口实现 */
void led_init(void) {
    memset(&m_effect, 0, sizeof(m_effect));
    k_timer_init(&m_timer, timer_handler, NULL);
    led_set_rainbow(12000, true);
    LOG_INF("LED initialized");
}

void led_set_rgb(int left_color, int right_color) {
    k_timer_stop(&m_timer);
    
    m_effect.state = EFFECT_SOLID;
    m_effect.colors[0] = (left_color >= 0) ? (uint32_t)left_color : 0;
    m_effect.colors[1] = (right_color >= 0) ? (uint32_t)right_color : 0;
    
    // 立即应用颜色
    apply_color(m_effect.colors[0], m_effect.colors[1]);
}

void led_set_breathing(int color, uint16_t period_ms, bool symmetric) {
    k_timer_stop(&m_timer);
    
    m_effect.state = EFFECT_BREATHING;
    m_effect.colors[0] = color;
    m_effect.period = MAX(period_ms, 1000);
    m_effect.symmetric = symmetric;
    m_effect.counter = 0;
    
    k_timer_start(&m_timer, K_MSEC(UPDATE_INTERVAL), K_MSEC(UPDATE_INTERVAL));
}

void led_set_rainbow(uint16_t period_ms, bool symmetric) {
    k_timer_stop(&m_timer);
    
    m_effect.state = EFFECT_RAINBOW;
    m_effect.period = MAX(period_ms, 2000);
    m_effect.symmetric = symmetric;
    m_effect.counter = 0;
    
    k_timer_start(&m_timer, K_MSEC(UPDATE_INTERVAL), K_MSEC(UPDATE_INTERVAL));
}

void led_set_blink(int color, uint16_t period_ms, bool symmetric) {
    k_timer_stop(&m_timer);
    
    m_effect.state = EFFECT_BLINK;
    m_effect.colors[0] = color;
    m_effect.period = MAX(period_ms, 200);
    m_effect.symmetric = symmetric;
    m_effect.counter = 0;
    
    k_timer_start(&m_timer, K_MSEC(UPDATE_INTERVAL), K_MSEC(UPDATE_INTERVAL));
}

void led_random_set(led_fun_effect_t effect)
{
    // 生成随机颜色索引
    int color_index = rand() % 360;
    int color = rainbow_table[color_index];

    switch (effect)
    {
        case LED_EFFECT_RGB:
        {
            led_set_rgb(color, color);
            break;
        }
        case LED_EFFECT_BREATHING:
            led_set_breathing(color, 2000, true);
            break;

        case LED_EFFECT_RAINBOW:
            led_set_rainbow(10000, true);
            break;

        case LED_EFFECT_BLINK:
            led_set_blink(color, 1000, true);
            break;

        default:
            break;
    }
}


void led_off(void) {
    k_timer_stop(&m_timer);
    m_effect.state = EFFECT_OFF;
    apply_color(0, 0);
}

/* 主循环处理 */
void led_process(void) {
    effect_update();
}
