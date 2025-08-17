#include <lvgl.h>
#include "screen.h"
#include "app_camera.h"
#include "app_obj_rec.h"
#include "app_wifi_qrcode_scan.h"
#include "app_chat.h"

// extern uint8_t aweui_camera_image_buffer[240 * 320 * 2];
uint8_t aweui_camera_image_buffer[240 * 240 * 2]
	__attribute__((section(".psram_section"))) = {0};

typedef struct {
    lv_obj_t *left_obj;             // 左屏图像对象
    lv_obj_t *right_obj;            // 右屏图像对象
    lv_img_dsc_t img_dsc;           // LVGL图像描述符
    uint8_t *frame_buf;             // 帧缓冲区
    camera_func_t camera_func;
    uint16_t tick;
} camera_screen_t;
static camera_screen_t camera_screen;

// RGB565 字节交换函数（大端转小端）
static void swap_rgb565_bytes(uint8_t *data, size_t size)
{
    for (size_t i = 0; i < size; i += 2) {
        uint8_t tmp = data[i];
        data[i] = data[i + 1];
        data[i + 1] = tmp;
    }
}

void obj_recognize()
{
    app_obj_rec_msg_send(APP_OBJ_REC_MSG_ID_UI_ENTER, NULL, 0);
    app_obj_rec_msg_send(APP_OBJ_REC_MSG_ID_UI_TAKE_PICTURE, NULL, 0);
	app_obj_rec_msg_send(APP_OBJ_REC_MSG_ID_UI_OBJ_RECOGNITION, NULL, 0);
}

void qrcode_recognize()
{
    app_chat_stop();

    app_wifi_qrcode_enter();

    k_msleep(10);
}

void camera_func_set(camera_func_t func)
{
    // switch (func)
    // {
    // case CAMERA_OBJ_REC:
    //     obj_recognize();
    //     break;
    // case CAMERA_QRCODE:
    //     qrcode_recognize();
    //     break;
    
    // default:
    //     break;
    // }

    camera_screen.camera_func = func;
}

void camera_func_change()
{
    camera_screen.camera_func++;
    if(camera_screen.camera_func >= CAMERA_FUNC_COUNT)
    {
        camera_screen.camera_func = CAMERA_SHOW;
    }
}

int camera_load()
{  
    // camera_screen.camera_func = CAMERA_SHOW;
        
    camera_screen.frame_buf = aweui_camera_image_buffer;

    camera_screen.right_obj = lv_img_create(screen_manager.screens[DISPLAY_RIGHT].root);
    lv_obj_set_size(camera_screen.right_obj, CAMERA_WIDTH, CAMERA_HEIGHT);
    lv_obj_align(camera_screen.right_obj, LV_ALIGN_CENTER, 0, 0);


    camera_screen.left_obj = lv_obj_create(screen_manager.screens[DISPLAY_LEFT].root);
    lv_obj_remove_style_all(camera_screen.left_obj);
    lv_obj_set_size(camera_screen.left_obj, 140, 140);
    lv_obj_set_style_radius(camera_screen.left_obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(camera_screen.left_obj, lv_color_hex(0xe9dbfc), 0);
    lv_obj_set_style_border_width(camera_screen.left_obj, 2, 0);                   // 设置边框宽度
    lv_obj_set_style_border_color(camera_screen.left_obj, lv_color_hex(0xe9dbfc), 0); // 设置边框颜色
    lv_obj_align(camera_screen.left_obj, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *lable = lv_label_create(camera_screen.left_obj);
    lv_label_set_text(lable, "camera show");      
    lv_obj_center(lable);   
    
    // 配置图像描述符
    camera_screen.img_dsc = (lv_img_dsc_t) {
        .header.always_zero = 0,
        .header.w = CAMERA_WIDTH,
        .header.h = CAMERA_HEIGHT,
        .header.cf = LV_IMG_CF_TRUE_COLOR, // RGB565格式
        .data_size =  (CAMERA_WIDTH * CAMERA_HEIGHT * 2), // RGB565 = 2 bytes/pixel,
        .data = camera_screen.frame_buf
    };

    // 启动摄像头
    camera_start();
    
    return 0;
}

void camera_show_frame()
{
    struct video_buffer *vbuf = NULL;
    int width = 0, height = 0, ret = 0;

    ret = app_camera_vbuf_capture(&vbuf, &width, &height);
    if (ret) {
        printk("Camera capture failed: %d\n", ret);
        return ;
    }
    memcpy(camera_screen.frame_buf, vbuf->buffer, (CAMERA_WIDTH * CAMERA_HEIGHT * 2));
    app_camera_vbuf_unref(vbuf);   // 释放视频缓冲区

    swap_rgb565_bytes(camera_screen.frame_buf, (CAMERA_WIDTH * CAMERA_HEIGHT * 2));     // 转换字节序 (RGB565 big-endian → little-endian)     

    lv_img_set_src(camera_screen.right_obj, &camera_screen.img_dsc);
    lv_obj_invalidate(camera_screen.right_obj);
    lv_timer_handler();
}

static int camera_update()
{
    switch (camera_screen.camera_func)
    {
    case CAMERA_SHOW:
        camera_screen.tick = 0;
        camera_show_frame();
        break;
    case CAMERA_TAKE_PIC:
        if(camera_screen.tick++ < 300)   
        {
            camera_show_frame();
        }
        break;
    case CAMERA_OBJ_REC:
        camera_screen.tick = 0;
        camera_show_frame();
        obj_recognize();
        camera_screen.camera_func = CAMERA_OBJ_REC_ING;
        break;
    case CAMERA_OBJ_REC_ING:
        if(camera_screen.tick++ > 1000)   
        {
            app_obj_rec_msg_send(APP_OBJ_REC_MSG_ID_UI_EXIT, NULL, 0);
            camera_screen.tick = 0;
            camera_screen.camera_func = CAMERA_SHOW;
        }
        break;
    case CAMERA_QRCODE:
        // camera_show_frame();
        camera_screen.camera_func = CAMERA_QRCODE_ING;
        break;
    case CAMERA_QRCODE_ING:
        // camera_show_frame();
        app_wifi_qrcode_msg_send(WIFI_QRCODE_EVT_PICTURE, NULL, 0, false);
        break;
    default:
        break;
    }

    return 0;
}

int camera_unload()
{
    camera_stop();

    camera_screen.tick = 0;

    if (camera_screen.right_obj != NULL) {
        lv_obj_del(camera_screen.right_obj);
        camera_screen.right_obj = NULL;
    }

    if (camera_screen.left_obj != NULL) {
        lv_obj_del(camera_screen.left_obj);
        camera_screen.left_obj = NULL;
    }

    return 0;
}


screen_interface_t camera_screen_interface = 
{
    .load = camera_load,
    .update = camera_update,
    .unload = camera_unload,
};

