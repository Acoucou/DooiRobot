#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <lvgl.h>
#include "screen.h"
#include "csk_malloc.h"
#include "action_sequences.h"
// #include "dooi_ui.h"

/* 设备树定义 */
#define DISPLAY_DEV_NODE DT_CHOSEN(zephyr_display)
#define GPIOB_7_NODE DT_ALIAS(gpiob7)

/* 硬件设备 */
static const struct gpio_dt_spec gpiob7 = GPIO_DT_SPEC_GET(GPIOB_7_NODE, gpios);
const struct device *display_dev = DEVICE_DT_GET(DISPLAY_DEV_NODE);
static const struct pwm_dt_spec lcd_pwm = PWM_DT_SPEC_GET(DT_NODELABEL(lcd_pwm));

/* 消息队列 */
K_MSGQ_DEFINE(display_flush_msgq, 0, 10, 4);

K_MUTEX_DEFINE(display_mutex);

/* 屏幕管理器实例 */
screen_manager_t screen_manager = {.current_display = DISPLAY_LEFT,
		.current_screen = SCREEN_NULL,
		.old_screen = SCREEN_NULL,
		.screens = {[DISPLAY_LEFT] = {.root = NULL, .needs_update = false, .interface = NULL},
				[DISPLAY_RIGHT] = {.root = NULL, .needs_update = false, .interface = NULL}}};

/* 屏幕接口实现 */
extern screen_interface_t digital_clock_screen_interface;
extern screen_interface_t emoji_gif_screen_interface;
extern screen_interface_t music_spectrum_screen_interface;
extern screen_interface_t eyes_screen_interface;
extern screen_interface_t camera_screen_interface;
extern screen_interface_t lucky_cat_screen_interface;
extern screen_interface_t info_settings_screen_interface;
extern screen_interface_t screen_select_screen_interface;
extern screen_interface_t wakeup_ack_screen_interface;

static screen_interface_t *screen_interfaces[SCREEN_COUNT] =
{       
    [SCREEN_DIGITAL_CLOCK] = &digital_clock_screen_interface,
	[SCREEN_EMOJI_GIF] = &emoji_gif_screen_interface,
	[SCREEN_MUSIC_SPECTRUM] = &music_spectrum_screen_interface,
	[SCREEN_EYES] = &eyes_screen_interface,
	[SCREEN_CAMERA] = &camera_screen_interface,
	[SCREEN_LUCKY_CAT] = &lucky_cat_screen_interface,
	[SCREEN_INFO] = &info_settings_screen_interface,
	[SCREEN_PAGE_SELECT] = &screen_select_screen_interface,
	[SCREEN_WAKEUP_ACK] = &wakeup_ack_screen_interface,
};

void screen_disp_mode_set(uint8_t mode)
{
	display_set_orientation(display_dev, mode);
}

void dooi_peripherals_init(screen_type_t screen_type)
{
    sequence_type_t seq_type;
    static uint8_t eyes_status = 0;

    switch (screen_type)
    {
    case SCREEN_EYES:
		switch(eyes_status)
		{
			case 0:
				seq_type = SEQUENCE_EYES_FIRST_WAKEUP;
				eyes_status = 1;
				break;
			case 1:
				seq_type = SEQUENCE_EYES_DEFAULT;
				eyes_status = 2;
				break;
			default:
				return;
				break;
		}
        break;
    
    case SCREEN_MUSIC_SPECTRUM:
        seq_type = SEQUENCE_MUSIC_DEFAULT;
        break;
    
    case SCREEN_LUCKY_CAT:
        seq_type = SEQUENCE_LUCKY_CAT_DEFAULT;
        break;
    
    case SCREEN_CAMERA:
        seq_type = SEQUENCE_CAMERA_DEFAULT;
        break;
    
    case SCREEN_DIGITAL_CLOCK:
        seq_type = SEQUENCE_CLOCK_DEFAULT;
        break;
    
    default:
        seq_type = SEQUENCE_GENERAL_DEFAULT;
        break;
    }
    
    // 将动作序列加入队列
    action_sequences_enqueue(seq_type);
}


void screen_sub_load()
{
	switch (screen_manager.current_screen)
	{
	case SCREEN_DIGITAL_CLOCK:
		digital_clock_mode_change();
		break;
	case SCREEN_EMOJI_GIF:
		emoji_gif_change();
		break;
	case SCREEN_EYES:
		// eyes_change();
		break;
	case SCREEN_CAMERA:
		camera_func_change();
		break;
	default:
		break;
	}
}

void screen_clear()
{
	screen_switch_display(DISPLAY_LEFT);
    k_msleep(50);
    display_blanking_off(display_dev);

    screen_switch_display(DISPLAY_RIGHT);
    k_msleep(50);
	display_blanking_off(display_dev);
}

/* 切换显示设备 */
void screen_switch_display(display_t display_id)
{
	if (display_id == DISPLAY_LEFT) {
		gpio_pin_configure_dt(&gpiob7, GPIO_OUTPUT_HIGH);
	} else {
		gpio_pin_configure_dt(&gpiob7, GPIO_OUTPUT_LOW);
	}
	screen_manager.current_display = display_id;
}

/* 设置亮度 */
int screen_set_brightness(uint8_t pwm_duty)
{
	if (lcd_pwm.flags == 1) {
		pwm_duty = 100 - pwm_duty;
	}

	pwm_duty = pwm_duty > 95 ? 95 : pwm_duty;
	pwm_duty = pwm_duty < 0 ? 0 : pwm_duty;

	if (!device_is_ready(lcd_pwm.dev)) {
		printk("device: %s is not ready\n", lcd_pwm.dev->name);
		return -ENODEV;
	}

	int r = pwm_set_dt(&lcd_pwm, lcd_pwm.period * 1000 * 100, pwm_duty * lcd_pwm.period * 1000);

	if (r) {
		printk("pwm set dt failed, r:%d\n", r);
	}
	return r;
}

/* 初始化UI系统 */
void screen_init(void)
{
	screen_set_brightness(0);

	if (!device_is_ready(display_dev)) {
		printk("display device is not ready\n");
		return;
	}

	lv_theme_t *theme = lv_theme_default_init(lv_disp_get_default(), lv_palette_main(LV_PALETTE_BLUE),
			lv_palette_main(LV_PALETTE_RED), true, LV_FONT_DEFAULT);

	screen_switch_display(DISPLAY_LEFT);
	lv_disp_set_theme(lv_disp_get_default(), theme);
	lv_timer_handler();

	screen_switch_display(DISPLAY_RIGHT);
	lv_disp_set_theme(lv_disp_get_default(), theme);
	lv_timer_handler();

	screen_manager.screens[DISPLAY_LEFT].root = lv_obj_create(NULL);
	screen_manager.screens[DISPLAY_RIGHT].root = lv_obj_create(NULL);
}

/* 设置下一个屏幕 */
void screen_set_current(screen_type_t screen_type)
{
	if(screen_type == SCREEN_WAKEUP_ACK)
	{
		screen_manager.tick = 0;
	}
	screen_manager.next_screen = screen_type;
}

void screen_load_next(void)
{
	int screen_type = screen_manager.current_screen;
	screen_type++;
    if(screen_type >= SCREEN_COUNT)
    {
        screen_type = SCREEN_DIGITAL_CLOCK;
    }
    screen_set_current(screen_type);
}
        

void screen_flush(void)
{
	/* 左屏更新 */
	if (screen_manager.screens[DISPLAY_LEFT].needs_update) {
		screen_manager.screens[DISPLAY_LEFT].needs_update = false;
		if (screen_manager.current_display != DISPLAY_LEFT) {
			screen_switch_display(DISPLAY_LEFT);
		}
		if (screen_manager.screens[DISPLAY_LEFT].root != NULL) {
			lv_scr_load(screen_manager.screens[DISPLAY_LEFT].root);
		}
		lv_timer_handler();
	}
	/* 右屏更新 */
	if (screen_manager.screens[DISPLAY_RIGHT].needs_update) {
		screen_manager.screens[DISPLAY_RIGHT].needs_update = false;
		if (screen_manager.current_display != DISPLAY_RIGHT) {
			screen_switch_display(DISPLAY_RIGHT);
		}
		if (screen_manager.screens[DISPLAY_RIGHT].root != NULL) {
			lv_scr_load(screen_manager.screens[DISPLAY_RIGHT].root);
		}
		lv_timer_handler();
	}
}

/* 加载屏幕 */
void screen_load(screen_type_t screen_type)
{
	if (screen_type >= SCREEN_COUNT || screen_type == SCREEN_NULL) {
		return;
	}

	if (screen_manager.current_screen != SCREEN_NULL && screen_interfaces[screen_manager.current_screen] != NULL) {
		screen_manager.screen_states[screen_manager.current_screen] = SCREEN_UNLOADED;

		printk("unload screen %d\r\n", screen_manager.current_screen);

		screen_interfaces[screen_manager.current_screen]->unload();
	}	
	
	printk("load screen %d\r\n", screen_type);

	/* 初始化新屏幕 */
	if (screen_interfaces[screen_type] != NULL) {
		screen_interfaces[screen_type]->load();
	}

	screen_manager.screens[DISPLAY_LEFT].needs_update = true;
    screen_manager.screens[DISPLAY_RIGHT].needs_update = true;

    screen_flush();

	screen_manager.screen_states[screen_type] = SCREEN_LOADED;
}

/* 更新屏幕 */
void screen_update(void)
{
	if (screen_manager.screen_states[screen_manager.current_screen] == SCREEN_LOADED && screen_interfaces[screen_manager.current_screen] != NULL) {
		screen_interfaces[screen_manager.current_screen]->update();
	}
}

/* 显示刷新线程 */
void app_display_flush(void *p1, void *p2, void *p3)
{
	while (1) {
		/* 等待刷新消息 */
		if (k_msgq_get(&display_flush_msgq, NULL, K_FOREVER) != 0) {
			continue;
		}

		k_mutex_lock(&display_mutex, K_FOREVER);

		screen_flush();

		k_mutex_unlock(&display_mutex);
	}
}


/* UI线程 */
void app_dooi_ui(void *p1, void *p2, void *p3)
{
	screen_init();
	screen_set_current(SCREEN_EYES);

	while (1) 
    {
		if (k_msgq_get(&display_flush_msgq, NULL, K_NO_WAIT) == 0) {
			screen_flush();
		}

		screen_update();

		if(screen_manager.current_screen == SCREEN_EMOJI_GIF || screen_manager.current_screen == SCREEN_WAKEUP_ACK)
		{
			screen_manager.tick++;
			if(screen_manager.tick > 200)
			{
				screen_manager.tick = 0;
				if(screen_manager.current_screen == SCREEN_EMOJI_GIF)
				{
					screen_set_current(SCREEN_EYES);
				}
				else
				{
					if(screen_manager.old_screen == SCREEN_INFO || screen_manager.old_screen == SCREEN_PAGE_SELECT)
					{
						screen_set_current(SCREEN_EYES);
					}
					else
					{
						screen_set_current(screen_manager.old_screen);	
					}
				}
			}
		}

		if(screen_manager.next_screen != SCREEN_NULL)
		{
			/* 屏幕切换处理 */
			if (screen_manager.current_screen != screen_manager.next_screen) 
			{
				screen_manager.old_screen = screen_manager.current_screen;
				dooi_peripherals_init(screen_manager.next_screen);
				screen_load(screen_manager.next_screen);
				screen_manager.current_screen = screen_manager.next_screen;
			}

			screen_manager.next_screen = SCREEN_NULL;
		}

		k_sleep(K_MSEC(10));
	}
}

static int screen_sys_init(void)
{
    k_thread_stack_t *stack;
    k_tid_t tid;
    const int stack_size = 12 * 1024;
    const int thread_prio = 6;

    stack = csk_aligned_alloc(8, stack_size);
    if (stack == NULL) {
        // LOG_ERR("screen stack malloc failed");
        return -ENOMEM;
    }

    struct k_thread *new_thread = csk_malloc(sizeof(struct k_thread));
    if (new_thread == NULL) {
        csk_free(stack);
        return -ENOMEM;
    }

    tid = k_thread_create(new_thread, stack, stack_size, app_dooi_ui, 
                         NULL, NULL, NULL, thread_prio, 0, K_NO_WAIT);
    if (tid != new_thread) {
        csk_free(new_thread);
        csk_free(stack);
        // LOG_ERR("app_dooi_ui thread creation failed");
        return -EFAULT;
    }

    k_thread_name_set(tid, "app_dooi_ui");
    return 0;
}

static int app_display_flush_init(void)
{
    k_thread_stack_t *stack;
    k_tid_t tid;
    const int stack_size = 3 * 1024;
    const int thread_prio = 6;

    stack = csk_aligned_alloc(8, stack_size);
    if (stack == NULL) {
        // LOG_ERR("screen stack malloc failed");
        return -ENOMEM;
    }

    struct k_thread *new_thread = csk_malloc(sizeof(struct k_thread));
    if (new_thread == NULL) {
        csk_free(stack);
        return -ENOMEM;
    }

    tid = k_thread_create(new_thread, stack, stack_size, app_display_flush, 
                         NULL, NULL, NULL, thread_prio, 0, K_NO_WAIT);
    if (tid != new_thread) {
        csk_free(new_thread);
        csk_free(stack);
        // LOG_ERR("app_display_flush thread creation failed");
        return -EFAULT;
    }

    k_thread_name_set(tid, "app_display_flush");
    return 0;
}

SYS_INIT(screen_sys_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
// SYS_INIT(app_display_flush_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
