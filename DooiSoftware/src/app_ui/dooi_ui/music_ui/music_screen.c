/**
 * @file lv_music_spectrum.c
 */

/*********************
 *      INCLUDES
 *********************/
#include <lvgl.h>
#include <stdlib.h>
#include <stdbool.h>
#include "spectrum_1.h"
#include "spectrum_2.h"
#include "spectrum_3.h"
#include "screen.h"

#define BAR_COLOR1          lv_color_hex(0xe9dbfc)
#define BAR_COLOR2          lv_color_hex(0x6f8af6)
#define BAR_COLOR3          lv_color_hex(0xffffff)
#define BAR_COLOR1_STOP     80
#define BAR_COLOR2_STOP     100
#define BAR_COLOR3_STOP     (2 * LV_HOR_RES / 3)
#define BAR_CNT             20
#define DEG_STEP            (180/BAR_CNT)
#define BAND_CNT            4
#define BAR_PER_BAND_CNT    (BAR_CNT / BAND_CNT)

#define CIRCLE_R   140

/*********************
 *      DEFINES
 *********************/
typedef struct {
    lv_obj_t * right_obj;
    lv_obj_t * left_obj;
    const uint16_t (*spectrum_data)[4];
    uint32_t spectrum_len;
    uint32_t spectrum_i;
    uint32_t spectrum_i_pause;
    uint32_t bar_ofs;
    uint32_t spectrum_lane_ofs_start;
    uint32_t bar_rot;
} lv_music_spectrum_t;

static lv_music_spectrum_t music_spectrum;

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void spectrum_draw_event_cb(lv_event_t * e);
static void spectrum_anim_cb(void * a, int32_t v);
static int32_t get_cos(int32_t deg, int32_t a);
static int32_t get_sin(int32_t deg, int32_t a);
void lv_music_spectrum_start(lv_music_spectrum_t * spectrum);
void lv_music_spectrum_set_data(lv_music_spectrum_t * spectrum, const uint16_t (*data)[4], uint32_t len);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

 /**
 * @brief 动画结束回调：随机选择新数据源并重启动画
 */
static void spectrum_ready_cb(lv_anim_t * anim)
{
    // 定义数据源数组
    static const struct {
        const uint16_t (*data)[4];
        uint32_t len;
    } sources[] = {
        {spectrum_1, sizeof(spectrum_1) / sizeof(spectrum_1[0])},
        {spectrum_2, sizeof(spectrum_2) / sizeof(spectrum_2[0])},
        {spectrum_3, sizeof(spectrum_3) / sizeof(spectrum_3[0])}
    };

    // 随机选择一个数据源
    int index = rand() % 3;
    lv_music_spectrum_set_data(&music_spectrum, sources[index].data, sources[index].len);

    // 重启动画（从新数据的0开始）
    lv_music_spectrum_start(&music_spectrum);
}


lv_music_spectrum_t * lv_music_spectrum_create(lv_obj_t * parent)
{
    lv_music_spectrum_t * spectrum = &music_spectrum;

    lv_memset_00(spectrum, sizeof(lv_music_spectrum_t));

    /*Create the spectrum visualizer*/
    spectrum->right_obj = lv_obj_create(parent);
    lv_obj_remove_style_all(spectrum->right_obj);
    lv_obj_set_size(spectrum->right_obj, 240, 240);
    lv_obj_add_event_cb(spectrum->right_obj, spectrum_draw_event_cb, LV_EVENT_ALL, spectrum);
    lv_obj_refresh_ext_draw_size(spectrum->right_obj);

    /*Create a circle in the center*/
    spectrum->left_obj = lv_obj_create(spectrum->right_obj);
    lv_obj_remove_style_all(spectrum->left_obj);
    lv_obj_set_size(spectrum->left_obj, CIRCLE_R, CIRCLE_R);
    lv_obj_set_style_radius(spectrum->left_obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(spectrum->left_obj, 2, 0);                   // 设置边框宽度
    lv_obj_set_style_border_color(spectrum->left_obj, lv_color_hex(0xe9dbfc), 0); // 设置边框颜色
    lv_obj_set_style_bg_color(spectrum->left_obj, lv_color_hex(0xe9dbfc), 0);
    lv_obj_align(spectrum->left_obj, LV_ALIGN_CENTER, 0, 0);

    spectrum->spectrum_i = 0;
    spectrum->spectrum_i_pause = 0;
    spectrum->bar_ofs = 0;
    spectrum->spectrum_lane_ofs_start = 0;
    spectrum->bar_rot = 0;

    return spectrum;
}

void lv_music_spectrum_set_data(lv_music_spectrum_t * spectrum, const uint16_t (*data)[4], uint32_t len)
{
    spectrum->spectrum_data = data;
    spectrum->spectrum_len = len;

    // 重置索引（切换数据时确保从头开始）
    spectrum->spectrum_i = 0;
    spectrum->spectrum_i_pause = 0;
    spectrum->spectrum_lane_ofs_start = 0;
}

void lv_music_spectrum_start(lv_music_spectrum_t * spectrum)
{
    spectrum->spectrum_i = 0;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_values(&a, spectrum->spectrum_i, spectrum->spectrum_len - 1);
    lv_anim_set_exec_cb(&a, spectrum_anim_cb);
    lv_anim_set_var(&a, spectrum);
    lv_anim_set_time(&a, ((spectrum->spectrum_len - spectrum->spectrum_i) * 1000) / 30);
    // lv_anim_set_playback_time(&a, 0);
    // lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE); // 设置无限循环
    lv_anim_set_ready_cb(&a, spectrum_ready_cb);
    lv_anim_start(&a);
}

void lv_music_spectrum_pause(lv_music_spectrum_t * spectrum)
{
    spectrum->spectrum_i_pause = spectrum->spectrum_i;
    spectrum->spectrum_i = 0;
    lv_anim_del(spectrum, spectrum_anim_cb);
    lv_obj_invalidate(spectrum->right_obj);
}

void lv_music_spectrum_resume(lv_music_spectrum_t * spectrum)
{
    spectrum->spectrum_i = spectrum->spectrum_i_pause;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_values(&a, spectrum->spectrum_i, spectrum->spectrum_len - 1);
    lv_anim_set_exec_cb(&a, spectrum_anim_cb);
    lv_anim_set_var(&a, spectrum);
    lv_anim_set_time(&a, ((spectrum->spectrum_len - spectrum->spectrum_i) * 1000) / 30);
    lv_anim_set_playback_time(&a, 0);
    lv_anim_start(&a);
}

void lv_music_spectrum_delete(lv_music_spectrum_t * spectrum)
{
    if(spectrum == NULL) return;
    lv_anim_del(spectrum, spectrum_anim_cb);
    lv_obj_del(spectrum->right_obj);
    lv_mem_free(spectrum);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void spectrum_draw_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_music_spectrum_t * spectrum = lv_event_get_user_data(e);

    if(code == LV_EVENT_REFR_EXT_DRAW_SIZE) {
        lv_event_set_ext_draw_size(e, LV_VER_RES);
    }
    else if(code == LV_EVENT_COVER_CHECK) {
        lv_event_set_cover_res(e, LV_COVER_RES_NOT_COVER);
    }
    else if(code == LV_EVENT_DRAW_POST) {
        lv_obj_t * obj = lv_event_get_target(e);
        lv_draw_ctx_t * draw_ctx = lv_event_get_draw_ctx(e);

        lv_opa_t opa = lv_obj_get_style_opa(obj, LV_PART_MAIN);
        if(opa < LV_OPA_MIN) return;

        lv_point_t poly[4];
        lv_point_t center;
        center.x = obj->coords.x1 + lv_obj_get_width(obj) / 2;
        center.y = obj->coords.y1 + lv_obj_get_height(obj) / 2;

        lv_draw_rect_dsc_t draw_dsc;
        lv_draw_rect_dsc_init(&draw_dsc);
        draw_dsc.bg_opa = LV_OPA_COVER;

        uint16_t r[64];
        uint32_t i;

        lv_coord_t min_a = 5;
        lv_coord_t r_in = CIRCLE_R / 2;  // Circle radius
        for(i = 0; i < BAR_CNT; i++) r[i] = r_in + min_a;

        uint32_t s;
        for(s = 0; s < 4; s++) {
            uint32_t f;
            uint32_t band_w = 0;    /*Real number of bars in this band.*/
            switch(s) {
                case 0:
                    band_w = 20;
                    break;
                case 1:
                    band_w = 8;
                    break;
                case 2:
                    band_w = 4;
                    break;
                case 3:
                    band_w = 2;
                    break;
            }

            /* Add "side bars" with cosine characteristic.*/
            for(f = 0; f < band_w; f++) {
                uint32_t ampl_main = spectrum->spectrum_data[spectrum->spectrum_i][s];
                int32_t ampl_mod = get_cos(f * 360 / band_w + 180, 180) + 180;
                int32_t t = BAR_PER_BAND_CNT * s - band_w / 2 + f;
                if(t < 0) t = BAR_CNT + t;
                if(t >= BAR_CNT) t = t - BAR_CNT;
                r[t] += (ampl_main * ampl_mod) >> 9;
            }
        }

        uint32_t amax = 20;
        int32_t animv = spectrum->spectrum_i - spectrum->spectrum_lane_ofs_start;
        if(animv > amax) animv = amax;
        for(i = 0; i < BAR_CNT; i++) {
            uint32_t deg_space = 1;
            uint32_t deg = i * DEG_STEP + 90;
            uint32_t j = (i + spectrum->bar_rot) % BAR_CNT;
            uint32_t k = (i + spectrum->bar_rot + 1) % BAR_CNT;

            uint32_t v = (r[k] * animv + r[j] * (amax - animv)) / amax;

            if(v < BAR_COLOR1_STOP) draw_dsc.bg_color = BAR_COLOR1;
            else if(v > BAR_COLOR3_STOP) draw_dsc.bg_color = BAR_COLOR3;
            else if(v > BAR_COLOR2_STOP) draw_dsc.bg_color = lv_color_mix(BAR_COLOR3, BAR_COLOR2,
                                                                              ((v - BAR_COLOR2_STOP) * 255) / (BAR_COLOR3_STOP - BAR_COLOR2_STOP));
            else draw_dsc.bg_color = lv_color_mix(BAR_COLOR2, BAR_COLOR1,
                                                      ((v - BAR_COLOR1_STOP) * 255) / (BAR_COLOR2_STOP - BAR_COLOR1_STOP));

            uint32_t di = deg + deg_space;

            int32_t x1_out = get_cos(di, v);
            poly[0].x = center.x + x1_out;
            poly[0].y = center.y + get_sin(di, v);

            int32_t x1_in = get_cos(di, r_in);
            poly[1].x = center.x + x1_in;
            poly[1].y = center.y + get_sin(di, r_in);
            di += DEG_STEP - deg_space * 2;

            int32_t x2_in = get_cos(di, r_in);
            poly[2].x = center.x + x2_in;
            poly[2].y = center.y + get_sin(di, r_in);

            int32_t x2_out = get_cos(di, v);
            poly[3].x = center.x + x2_out;
            poly[3].y = center.y + get_sin(di, v);

            lv_draw_polygon(draw_ctx, &draw_dsc, poly, 4);

            poly[0].x = center.x - x1_out;
            poly[1].x = center.x - x1_in;
            poly[2].x = center.x - x2_in;
            poly[3].x = center.x - x2_out;
            lv_draw_polygon(draw_ctx, &draw_dsc, poly, 4);
        }
    }
}

static void spectrum_anim_cb(void * a, int32_t v)
{
    lv_music_spectrum_t * spectrum = a;
    spectrum->spectrum_i = v;
    lv_obj_invalidate(spectrum->right_obj);

    static uint32_t bass_cnt = 0;
    static int32_t last_bass = -1000;
    if(spectrum->spectrum_data[spectrum->spectrum_i][0] > 12) {
        if(spectrum->spectrum_i - last_bass > 5) {
            bass_cnt++;
            last_bass = spectrum->spectrum_i;
            if(bass_cnt >= 2) {
                bass_cnt = 0;
                spectrum->spectrum_lane_ofs_start = spectrum->spectrum_i;
                spectrum->bar_ofs++;
            }
        }
    }
    if(spectrum->spectrum_data[spectrum->spectrum_i][0] < 4) spectrum->bar_rot += 1;
}

static inline int32_t get_cos(int32_t deg, int32_t a) {
    deg = deg % 360;
    return (lv_trigo_cos(deg) * a + LV_TRIGO_SIN_MAX/2) >> LV_TRIGO_SHIFT;
}

static inline int32_t get_sin(int32_t deg, int32_t a) {
    deg = deg % 360;
    return (lv_trigo_sin(deg) * a + LV_TRIGO_SIN_MAX/2) >> LV_TRIGO_SHIFT;
}

// static int32_t get_cos(int32_t deg, int32_t a)
// {
//     int32_t r = (lv_trigo_cos(deg) * a);
//     r += LV_TRIGO_SIN_MAX / 2;
//     return r >> LV_TRIGO_SHIFT;
// }

// static int32_t get_sin(int32_t deg, int32_t a)
// {
//     int32_t r = lv_trigo_sin(deg) * a;
//     r += LV_TRIGO_SIN_MAX / 2;
//     return r >> LV_TRIGO_SHIFT;
// }


int spectrum_load()
{    
    lv_obj_t *screen_left = screen_manager.screens[DISPLAY_LEFT].root;
    lv_obj_t *screen_right = screen_manager.screens[DISPLAY_RIGHT].root;

    lv_memset_00(&music_spectrum, sizeof(lv_music_spectrum_t));
    /*Create the spectrum visualizer*/
    music_spectrum.right_obj = lv_obj_create(screen_right);
    lv_obj_remove_style_all(music_spectrum.right_obj);
    lv_obj_set_size(music_spectrum.right_obj, 240, 240);
    lv_obj_add_event_cb(music_spectrum.right_obj, spectrum_draw_event_cb, LV_EVENT_ALL, &music_spectrum);
    lv_obj_refresh_ext_draw_size(music_spectrum.right_obj);

    /*Create a circle in the center*/
    lv_obj_t *circle = lv_obj_create(music_spectrum.right_obj);
    lv_obj_remove_style_all(circle);
    lv_obj_set_size(circle, CIRCLE_R, CIRCLE_R);
    lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(circle, 2, 0);                   // 设置边框宽度
    lv_obj_set_style_border_color(circle, lv_color_hex(0xe9dbfc), 0); // 设置边框颜色
    lv_obj_set_style_bg_color(circle, lv_color_hex(0xe9dbfc), 0);
    lv_obj_align(circle, LV_ALIGN_CENTER, 0, 0);
    music_spectrum.spectrum_i = 0;
    music_spectrum.spectrum_i_pause = 0;
    music_spectrum.bar_ofs = 0;
    music_spectrum.spectrum_lane_ofs_start = 0;
    music_spectrum.bar_rot = 0;

    music_spectrum.left_obj = lv_obj_create(screen_left);
    lv_obj_remove_style_all(music_spectrum.left_obj);
    lv_obj_set_size(music_spectrum.left_obj, CIRCLE_R, CIRCLE_R);
    lv_obj_set_style_radius(music_spectrum.left_obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(music_spectrum.left_obj, lv_color_hex(0xe9dbfc), 0);
    lv_obj_set_style_border_width(music_spectrum.left_obj, 2, 0);                   // 设置边框宽度
    lv_obj_set_style_border_color(music_spectrum.left_obj, lv_color_hex(0xe9dbfc), 0); // 设置边框颜色
    lv_obj_align(music_spectrum.left_obj, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *lable = lv_label_create(music_spectrum.left_obj);
    lv_label_set_text(lable, "music spectrum");      
    lv_obj_center(lable);   
    
    // 定义数据源数组
    static const struct {
        const uint16_t (*data)[4];
        uint32_t len;
    } sources[] = {
        {spectrum_1, sizeof(spectrum_1) / sizeof(spectrum_1[0])},
        {spectrum_2, sizeof(spectrum_2) / sizeof(spectrum_2[0])},
        {spectrum_3, sizeof(spectrum_3) / sizeof(spectrum_3[0])}
    };

    // 初始随机选择一个数据源
    int index = rand() % 3;

    lv_music_spectrum_set_data(&music_spectrum, sources[index].data, sources[index].len);
    // 重启动画
    lv_music_spectrum_start(&music_spectrum);

    return 0;
}

static int spectrum_update()
{
    lv_timer_handler();

    return 0;
}

int spectrum_unload()
{
    lv_music_spectrum_pause(&music_spectrum);

    if (music_spectrum.right_obj != NULL) {
        lv_obj_del(music_spectrum.right_obj);
        music_spectrum.right_obj = NULL;
    }

    if (music_spectrum.left_obj != NULL) {
        lv_obj_del(music_spectrum.left_obj);
        music_spectrum.left_obj = NULL;
    }

    return 0;
}

screen_interface_t music_spectrum_screen_interface = 
{
    .load = spectrum_load,
    .update = spectrum_update,
    .unload = spectrum_unload,
};