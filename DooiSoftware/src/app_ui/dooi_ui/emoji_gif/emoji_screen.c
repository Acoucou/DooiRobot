#include <lvgl.h>
#include <zephyr/kernel.h>
#include "screen.h"

/* GIF动画资源结构 */
typedef struct {
    const lv_img_dsc_t* left;   // 左眼图片
    const lv_img_dsc_t* right;  // 右眼图片
} gif_animation_t;

/* GIF屏幕实例数据 */
typedef struct {
    gif_animation_type_t current_animation;
    lv_obj_t *left_image;
    lv_obj_t *right_image;
} gif_screen_data_t;

/* 资源声明 */
LV_IMG_DECLARE(close_eyes_slow);
LV_IMG_DECLARE(angle_left);
LV_IMG_DECLARE(angle_right);
LV_IMG_DECLARE(sad_left);
LV_IMG_DECLARE(sad_right);
LV_IMG_DECLARE(disdain_left);
LV_IMG_DECLARE(disdain_right);
LV_IMG_DECLARE(excited);
LV_IMG_DECLARE(fear_left);
LV_IMG_DECLARE(fear_right);
LV_IMG_DECLARE(left);
LV_IMG_DECLARE(right);
LV_IMG_DECLARE(close_eyes_quick);

/* 动画资源数组 - 使用枚举作为索引 */
static const gif_animation_t animations[] = {
    [ANIMATION_CLOSE_EYES_SLOW] = {.left = &close_eyes_slow, .right = &close_eyes_slow},
    [ANIMATION_ANGRY] = { .left = &angle_left, .right = &angle_right },
    [ANIMATION_EXCITED] = { .left = &excited, .right = &excited },
    [ANIMATION_FEAR]   = { .left = &fear_left,   .right = &fear_right },
    [ANIMATION_DISDAIN] = { .left = &disdain_left, .right = &disdain_right },
    [ANIMATION_LEFT]   = { .left = &left,   .right = &left },
    [ANIMATION_RIGHT]  = { .left = &right,  .right = &right },
    [ANIMATION_CLOSE_EYES_QUICK] = { .left = &close_eyes_quick, .right = &close_eyes_quick },
    [ANIMATION_SAD]   = { .left = &sad_left,   .right = &sad_right },
};


/* 当前屏幕实例 */
static gif_screen_data_t emoji_data;


/* 切换GIF动画 */
void switch_gif(int animation)
{
	k_mutex_lock(&display_mutex, K_FOREVER);

    if (animation >= ANIMATION_COUNT) return;

    // 更新当前动画类型
    emoji_data.current_animation = animation;
    
    // 立即更新显示
    if (emoji_data.left_image && emoji_data.right_image) {
        lv_gif_set_src(emoji_data.left_image, animations[animation].left);
        lv_gif_set_src(emoji_data.right_image, animations[animation].right);
    }

	k_mutex_unlock(&display_mutex);
} 

void emoji_gif_change()
{
    emoji_data.current_animation ++;
    if(emoji_data.current_animation >= ANIMATION_COUNT)
    {
        emoji_data.current_animation = ANIMATION_CLOSE_EYES_SLOW;
    }
    switch_gif(emoji_data.current_animation);
}

static int emoji_gif_load(void)
{
    lv_obj_t *screen_left = screen_manager.screens[DISPLAY_LEFT].root;
    lv_obj_t *screen_right = screen_manager.screens[DISPLAY_RIGHT].root;

    // memset(&emoji_data, 0, sizeof(emoji_data));
    // emoji_data.current_animation = ANIMATION_CLOSE_EYES_SLOW;

    // 创建GIF控件
    emoji_data.left_image = lv_gif_create(screen_left);
    emoji_data.right_image = lv_gif_create(screen_right);
    
    if (!emoji_data.left_image || !emoji_data.right_image)
        return -ENOMEM;

    // 设置初始动画
    switch_gif(emoji_data.current_animation);
    
    // 居中显示
    lv_obj_align(emoji_data.left_image, LV_ALIGN_CENTER, 0, 0);
    lv_obj_align(emoji_data.right_image, LV_ALIGN_CENTER, 0, 0);
    
    return 0;
}


static int emoji_gif_update(void)
{
    screen_manager.screens[DISPLAY_LEFT].needs_update = true;
    screen_manager.screens[DISPLAY_RIGHT].needs_update = true;
    k_msgq_put(&display_flush_msgq, NULL, K_NO_WAIT);

    return 0;
}

static int emoji_gif_unload(void)
{
    // 删除图像对象
    if (emoji_data.left_image != NULL) {
        lv_obj_del(emoji_data.left_image);
        emoji_data.left_image = NULL;
    }
    
    if (emoji_data.right_image != NULL) {
        lv_obj_del(emoji_data.right_image);
        emoji_data.right_image = NULL;
    }

    return 0;
}

/* 屏幕接口 */
screen_interface_t emoji_gif_screen_interface = {
    .load = emoji_gif_load,
    .update = emoji_gif_update,
    .unload = emoji_gif_unload,
};
