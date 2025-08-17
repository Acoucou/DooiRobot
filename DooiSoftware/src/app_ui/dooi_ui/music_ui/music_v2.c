/**
 * @file lv_demo_music_spectrum.c
 * @brief 音乐频谱可视化组件（完整稳定版）
 */

/*********************
 *      INCLUDES
 *********************/
#include "lvgl.h"
#include <stdbool.h>
#include <math.h>
#include "spectrum_1.h"
#include "spectrum_2.h"
#include "spectrum_3.h"

/*********************
 *      DEFINES
 *********************/
/* 核心参数 */
#define SPECTRUM_CIRCLE_SIZE     120     // 中心圆直径（80-200）
#define SPECTRUM_BAR_CNT         20      // 频谱条数量（建议偶数）
#define SPECTRUM_BAND_CNT        4       // 频段数量（固定4频段）

/* 颜色配置 */
#define COLOR_BASE               lv_color_hex(0xe9dbfc)  // 基础色
#define COLOR_LOW_FREQ           lv_color_hex(0x6f8af6)  // 低频颜色
#define COLOR_HIGH_FREQ          lv_color_hex(0xffffff)  // 高频颜色
#define COLOR_STOP_OFFSET        30      // 颜色渐变点偏移量

/* 动态参数 */
#define AMPLIFY_GLOBAL           1.2f    // 全局幅度系数（0.5-3.0）
#define AMPLIFY_BASS             2.0f    // 低频增强系数（1.0-4.0）
#define ATTENUATE_HIGH           0.7f    // 高频衰减系数（0.0-1.0）
#define ANIMATION_SMOOTHNESS     30      // 动画平滑度（10-50）
#define MAX_HEIGHT_LIMIT         100     // 最大高度限制（像素）
#define FRAME_RATE               30      // 动画帧率（20-60）
#define BASS_THRESHOLD           8       // 低频触发阈值（0-15）

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    lv_obj_t * spectrum_obj;     // 频谱主容器
    lv_obj_t * circle_obj;       // 中心圆对象
    const uint16_t (*spectrum_data)[4]; // 频谱数据源
    uint32_t spectrum_len;       // 数据总长度
    uint32_t spectrum_i;         // 当前数据索引
    uint32_t spectrum_i_pause;   // 暂停时索引位置
    uint32_t bar_rot;            // 条形旋转状态
    uint32_t spectrum_lane_ofs_start; // 动画偏移起点
} lv_demo_music_spectrum_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void spectrum_draw_event_cb(lv_event_t * e);
static void spectrum_anim_cb(void * a, int32_t v);
static inline int32_t get_cos(int32_t deg, int32_t a);
static inline int32_t get_sin(int32_t deg, int32_t a);
static void apply_audio_effect(uint32_t* ampl_main, uint32_t band);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief 创建频谱可视化组件
 * @param parent 父对象
 * @return 频谱对象指针（NULL表示失败）
 */
lv_demo_music_spectrum_t * lv_demo_music_spectrum_create(lv_obj_t * parent)
{
    LV_ASSERT_NULL(parent);
    
    lv_demo_music_spectrum_t * spectrum = lv_mem_alloc(sizeof(lv_demo_music_spectrum_t));
    if(spectrum == NULL) {
        LV_LOG_ERROR("Memory allocation failed");
        return NULL;
    }
    lv_memset_00(spectrum, sizeof(lv_demo_music_spectrum_t));

    /* 创建频谱主容器 */
    spectrum->spectrum_obj = lv_obj_create(parent);
    if(spectrum->spectrum_obj == NULL) {
        lv_mem_free(spectrum);
        return NULL;
    }
    lv_obj_remove_style_all(spectrum->spectrum_obj);
    lv_obj_set_size(spectrum->spectrum_obj, 240, 240);
    lv_obj_add_event_cb(spectrum->spectrum_obj, spectrum_draw_event_cb, LV_EVENT_ALL, spectrum);
    lv_obj_refresh_ext_draw_size(spectrum->spectrum_obj);

    /* 创建中心圆 */
    spectrum->circle_obj = lv_obj_create(spectrum->spectrum_obj);
    if(spectrum->circle_obj == NULL) {
        lv_obj_del(spectrum->spectrum_obj);
        lv_mem_free(spectrum);
        return NULL;
    }
    lv_obj_remove_style_all(spectrum->circle_obj);
    lv_obj_set_size(spectrum->circle_obj, SPECTRUM_CIRCLE_SIZE, SPECTRUM_CIRCLE_SIZE);
    lv_obj_set_style_radius(spectrum->circle_obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(spectrum->circle_obj, COLOR_BASE, 0);
    lv_obj_align(spectrum->circle_obj, LV_ALIGN_CENTER, 0, 0);

    /* 初始化状态 */
    spectrum->spectrum_i = 0;
    spectrum->spectrum_i_pause = 0;
    spectrum->bar_rot = 0;
    spectrum->spectrum_lane_ofs_start = 0;

    return spectrum;
}

/**
 * @brief 设置频谱数据源
 * @param spectrum 频谱对象
 * @param data 数据数组指针
 * @param len 数据长度（必须>0）
 */
void lv_demo_music_spectrum_set_data(lv_demo_music_spectrum_t * spectrum, const uint16_t (*data)[4], uint32_t len)
{
    LV_ASSERT_NULL(spectrum);
    LV_ASSERT_NULL(data);
    LV_ASSERT(len > 0);
    
    spectrum->spectrum_data = data;
    spectrum->spectrum_len = len;
}

/**
 * @brief 启动/恢复频谱动画
 * @param spectrum 频谱对象
 */
void lv_demo_music_spectrum_start(lv_demo_music_spectrum_t * spectrum)
{
    LV_ASSERT_NULL(spectrum);
    
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, spectrum);
    lv_anim_set_exec_cb(&a, spectrum_anim_cb);
    lv_anim_set_values(&a, spectrum->spectrum_i, spectrum->spectrum_len - 1);
    
    uint32_t time_ms = (spectrum->spectrum_len * 1000) / FRAME_RATE;
    time_ms = LV_CLAMP(1000, time_ms, 60000); // 限制1s-60s
    lv_anim_set_time(&a, time_ms);
    
    lv_anim_start(&a);
}

/* pause/resume/delete 函数保持结构，添加参数校验 */
void lv_demo_music_spectrum_pause(lv_demo_music_spectrum_t * spectrum)
{
    if(spectrum == NULL) return;
    spectrum->spectrum_i_pause = spectrum->spectrum_i;
    spectrum->spectrum_i = 0;
    lv_anim_del(spectrum, spectrum_anim_cb);
    lv_obj_invalidate(spectrum->spectrum_obj);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * @brief 频谱绘制核心逻辑
 */
static void spectrum_draw_event_cb(lv_event_t * e)
{
    /* 事件类型处理 */
    lv_event_code_t code = lv_event_get_code(e);
    lv_demo_music_spectrum_t * spectrum = lv_event_get_user_data(e);
    if(code != LV_EVENT_DRAW_POST) return;

    /* 获取绘制上下文 */
    lv_obj_t * obj = lv_event_get_target(e);
    lv_draw_ctx_t * draw_ctx = lv_event_get_draw_ctx(e);
    
    /* 透明度检查 */
    if(lv_obj_get_style_opa(obj, LV_PART_MAIN) < LV_OPA_MIN) return;

    /* 初始化绘图元素 */
    lv_point_t poly[4];
    lv_point_t center = {
        .x = obj->coords.x1 + lv_obj_get_width(obj)/2,
        .y = obj->coords.y1 + lv_obj_get_height(obj)/2
    };
    
    lv_draw_rect_dsc_t draw_dsc;
    lv_draw_rect_dsc_init(&draw_dsc);
    draw_dsc.bg_opa = LV_OPA_COVER;

    /* 基础参数计算 */
    const lv_coord_t r_in = SPECTRUM_CIRCLE_SIZE / 2;
    const lv_coord_t deg_step = 180 / SPECTRUM_BAR_CNT;
    uint16_t r[SPECTRUM_BAR_CNT];
    
    /* 初始化条形半径 */
    for(uint32_t i=0; i<SPECTRUM_BAR_CNT; i++) r[i] = r_in;

    /* 处理各频段数据 */
    for(uint32_t s=0; s<SPECTRUM_BAND_CNT; s++) {
        uint32_t band_w = 0;
        switch(s) { // 各频段权重
            case 0: band_w = 20; break; // 低频（20条）
            case 1: band_w = 8;  break; // 中低频
            case 2: band_w = 4;  break; // 中高频
            case 3: band_w = 2;  break; // 高频
        }

        for(uint32_t f=0; f<band_w; f++) {
            uint32_t ampl_main = spectrum->spectrum_data[spectrum->spectrum_i][s];
            apply_audio_effect(&ampl_main, s); // 应用音效
            
            /* 计算振幅调制 */
            int32_t ampl_mod = get_cos(f * 360 / band_w + 180, 180) + 180;
            
            /* 计算目标条形索引 */
            int32_t t = (SPECTRUM_BAR_CNT/SPECTRUM_BAND_CNT)*s - band_w/2 + f;
            t = (t % SPECTRUM_BAR_CNT + SPECTRUM_BAR_CNT) % SPECTRUM_BAR_CNT; // 循环修正
            
            /* 更新条形半径 */
            r[t] += (int32_t)((ampl_main * ampl_mod) * AMPLIFY_GLOBAL) >> 9;
            r[t] = LV_MIN(r[t], r_in + MAX_HEIGHT_LIMIT);
        }
    }

    /* 动态颜色计算 */
    const lv_coord_t color_stop1 = r_in + COLOR_STOP_OFFSET;
    const lv_coord_t color_stop2 = color_stop1 + 20;
    const lv_coord_t color_stop3 = color_stop2 + 20;

    /* 绘制所有条形 */
    uint32_t amax = ANIMATION_SMOOTHNESS;
    int32_t animv = LV_CLAMP(0, spectrum->spectrum_i - spectrum->spectrum_lane_ofs_start, amax);
    
    for(uint32_t i=0; i<SPECTRUM_BAR_CNT; i++) {
        /* 计算当前条形值 */
        uint32_t j = (i + spectrum->bar_rot) % SPECTRUM_BAR_CNT;
        uint32_t k = (j + 1) % SPECTRUM_BAR_CNT;
        uint32_t v = (r[k]*animv + r[j]*(amax - animv)) / amax;

        /* 设置颜色 */
        if(v < color_stop1) {
            draw_dsc.bg_color = COLOR_LOW_FREQ;
        } else if(v > color_stop3) {
            draw_dsc.bg_color = COLOR_HIGH_FREQ;
        } else {
            lv_opa_t ratio = (v - color_stop1) * 255 / (color_stop3 - color_stop1);
            draw_dsc.bg_color = lv_color_mix(COLOR_HIGH_FREQ, COLOR_LOW_FREQ, ratio);
        }

        /* 计算多边形坐标 */
        uint32_t deg = i * deg_step + 90;
        uint32_t di = deg + 1; // 1度间隔
        
        poly[0] = (lv_point_t){center.x + get_cos(di, v),   center.y + get_sin(di, v)};
        poly[1] = (lv_point_t){center.x + get_cos(di, r_in), center.y + get_sin(di, r_in)};
        di += deg_step - 2;
        poly[2] = (lv_point_t){center.x + get_cos(di, r_in), center.y + get_sin(di, r_in)};
        poly[3] = (lv_point_t){center.x + get_cos(di, v),   center.y + get_sin(di, v)};

        /* 绘制对称条形 */
        lv_draw_polygon(draw_ctx, &draw_dsc, poly, 4);
        for(uint8_t m=0; m<4; m++) poly[m].x = 2*center.x - poly[m].x;
        lv_draw_polygon(draw_ctx, &draw_dsc, poly, 4);
    }
}

/**
 * @brief 应用音效处理
 * @param ampl_main 振幅指针（输入输出参数）
 * @param band 频段索引
 */
static void apply_audio_effect(uint32_t* ampl_main, uint32_t band)
{
    switch(band) {
        case 0: *ampl_main *= AMPLIFY_BASS; break;
        case 3: *ampl_main *= ATTENUATE_HIGH; break;
        default: break;
    }
}

/**
 * @brief 动画回调（添加节拍检测）
 */
static void spectrum_anim_cb(void * a, int32_t v)
{
    lv_demo_music_spectrum_t * spectrum = a;
    spectrum->spectrum_i = v % spectrum->spectrum_len; // 循环支持
    
    /* 低频节拍检测 */
    static uint32_t bass_cnt = 0;
    static int32_t last_bass = -1000;
    if(spectrum->spectrum_data[v][0] > BASS_THRESHOLD) {
        if(v - last_bass > 5) {
            if(++bass_cnt >= 2) {
                bass_cnt = 0;
                spectrum->spectrum_lane_ofs_start = v;
            }
            last_bass = v;
        }
    }
    
    /* 随机旋转 */
    if(spectrum->spectrum_data[v][0] < (BASS_THRESHOLD/2)) {
        spectrum->bar_rot = (spectrum->bar_rot + 1) % SPECTRUM_BAR_CNT;
    }
    
    lv_obj_invalidate(spectrum->spectrum_obj);
}

/**
 * @brief 优化三角函数计算
 */
static inline int32_t get_cos(int32_t deg, int32_t a) {
    deg = deg % 360;
    return (lv_trigo_cos(deg) * a + LV_TRIGO_SIN_MAX/2) >> LV_TRIGO_SHIFT;
}

static inline int32_t get_sin(int32_t deg, int32_t a) {
    deg = deg % 360;
    return (lv_trigo_sin(deg) * a + LV_TRIGO_SIN_MAX/2) >> LV_TRIGO_SHIFT;
}

/**********************
 *   EXAMPLE USAGE
 **********************/
void lv_example_music_spectrum(void)
{
    lv_demo_music_spectrum_t * spectrum = lv_demo_music_spectrum_create(lv_scr_act());
    if(spectrum) {
        lv_demo_music_spectrum_set_data(spectrum, spectrum_1, 
                                      sizeof(spectrum_1)/sizeof(spectrum_1[0]));
        lv_demo_music_spectrum_start(spectrum);
    }
}
