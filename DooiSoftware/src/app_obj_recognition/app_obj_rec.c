#include "stdint.h"
#include "zephyr/init.h"
#include <stdbool.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_obj_rec_logger, LOG_LEVEL_DBG);

#include "csk_malloc.h"
// #include "obj_rec_session.h"
#include "app_camera.h"

#define NDEBUG
#define TJE_IMPLEMENTATION
#include "tiny_jpeg.h"

#include "app_obj_rec.h"
#include "lsc_objrec.h"
enum {
	APP_OBJ_REC_RUN_MODE_OFF = 0,
	APP_OBJ_REC_RUN_MODE_PREVIEW,
	APP_OBJ_REC_RUN_MODE_PICTURE_TAKE,
};

struct jpeg_write_context {
	uint8_t *buf;
	uint32_t total_size;
	uint32_t wrote;
};

// extern void app_obj_rec_ui_evt_notify(uint32_t evt, void *data, uint32_t size);

typedef int (*app_obj_rec_msg_handle_t)(uint32_t id, void *data, uint32_t len);
static int app_obj_rec_take_picture_handle(uint32_t id, void *data, uint32_t len);
static int app_obj_rec_exit_handle(uint32_t id, void *data, uint32_t len);
static int app_obj_rec_enter_handle(uint32_t id, void *data, uint32_t len);
static int app_obj_rec_back_to_preview_handle(uint32_t id, void *data, uint32_t len);
static int app_obj_rec_obj_recognition_handle(uint32_t id, void *data, uint32_t len);

static const app_obj_rec_msg_handle_t app_obj_rec_msg_handles[APP_OBJ_REC_MSG_ID_MAX] = {
	[APP_OBJ_REC_MSG_ID_UI_START] = app_obj_rec_enter_handle,
	[APP_OBJ_REC_MSG_ID_UI_TAKE_PICTURE] = app_obj_rec_take_picture_handle,
	[APP_OBJ_REC_MSG_ID_UI_BACK_TO_PREVIEW] = app_obj_rec_back_to_preview_handle,
	[APP_OBJ_REC_MSG_ID_UI_OBJ_RECOGNITION] = app_obj_rec_obj_recognition_handle,
	[APP_OBJ_REC_MSG_ID_UI_EXIT] = app_obj_rec_exit_handle,
};

static uint8_t app_obj_rec_started = false;
static uint8_t app_obj_rec_run_mode = APP_OBJ_REC_RUN_MODE_PREVIEW;
static struct video_buffer *vbuf_capture = NULL;

K_MSGQ_DEFINE(app_obj_rec_msgq, sizeof(struct app_obj_rec_msg), 20, 4);

enum {
	APP_OBJ_REC_EVT_ACTION_BACK_TO_PREVIEW = BIT(0),
	APP_OBJ_REC_EVT_ACTION_REC_RUN = BIT(1),
	APP_OBJ_REC_EVT_ACTION_UNKNOWN = BIT(2),
};

K_EVENT_DEFINE(app_obj_rec_evt_ui_action);

void app_obj_rec_msg_send(uint32_t id, void *data, uint32_t len)
{
	int err;
	struct app_obj_rec_msg msg = {
		.id = id,
		.data = data,
		.len = len,
	};
	err = k_msgq_put(&app_obj_rec_msgq, &msg, K_NO_WAIT);
	if (err) {
		LOG_ERR("app main msg send failed, err:%d", err);
	}
}

static inline void camera_rgb565_swap(uint8_t *buffer, uint32_t size)
{
	for (uint32_t i = 0; i < size; i += 2) {
		uint16_t *pixel = (uint16_t *)(buffer + i);
		*pixel = __builtin_bswap16(*pixel);
	}
}

static int app_obj_rec_take_picture_handle(uint32_t id, void *data, uint32_t len)
{
	app_obj_rec_run_mode = APP_OBJ_REC_RUN_MODE_PICTURE_TAKE;

	int err;
	int w, h;

	vbuf_capture = NULL;
	err = app_camera_vbuf_capture(&vbuf_capture, &w, &h);
	if (err) {
		LOG_ERR("app_camera_vbuf_capture failed, err:%d", err);
		return err;
	}

	camera_rgb565_swap(vbuf_capture->buffer, vbuf_capture->bytesused);

	// app_obj_rec_ui_evt_notify(APP_OBJ_REC_EVT_UI_PICTURE_DISPLAY, vbuf_capture->buffer,
	// 			  vbuf_capture->bytesused);

	return 0;
}

static int app_obj_rec_back_to_preview_handle(uint32_t id, void *data, uint32_t len)
{
	app_obj_rec_run_mode = APP_OBJ_REC_RUN_MODE_PREVIEW;

	if (vbuf_capture) {
		app_camera_vbuf_unref(vbuf_capture);
		vbuf_capture = NULL;
	}

	return 0;
}

static void rgb565_to_rgb888_le(uint16_t *src, uint8_t *dst, size_t pixel_count)
{
	for (size_t i = 0; i < pixel_count; i++) {
		uint16_t pixel = src[i];
		uint8_t r = (pixel >> 11) & 0x1F;
		uint8_t g = (pixel >> 5) & 0x3F;
		uint8_t b = pixel & 0x1F;

		dst[i * 3 + 0] = (r * 255) / 31;
		dst[i * 3 + 1] = (g * 255) / 63;
		dst[i * 3 + 2] = (b * 255) / 31;
	}
}

static void jpeg_write(void *context, void *data, int size)
{
	struct jpeg_write_context *ctx = (struct jpeg_write_context *)context;
	if (ctx == NULL) {
		return;
	}

	uint32_t can_write = ctx->total_size - ctx->wrote;
	can_write = can_write >= size ? size : can_write;

	memcpy(ctx->buf + ctx->wrote, data, can_write);

	ctx->wrote += can_write;
}

/**
 * @note the picture must be formatted as rgb565
 */
static int app_obj_rec_obj_recognition(void *pic, uint32_t w, uint32_t h)
{
	uint8_t *rgb888_buf;
	uint32_t rgb888_buf_size;
	uint8_t *jpeg_buf;
	uint32_t jpeg_buf_size;
	int err;

	rgb888_buf_size = w * h * 3;
	jpeg_buf_size = rgb888_buf_size / 5;

	/* convert picture to rgb888 */
	rgb888_buf = csk_malloc(rgb888_buf_size);
	if (rgb888_buf == NULL) {
		LOG_ERR("no more mem to alloc rgb888 buffer, size:%d", rgb888_buf_size);
		return -ENOMEM;
	}
	jpeg_buf = csk_malloc(jpeg_buf_size);
	if (jpeg_buf == NULL) {
		LOG_ERR("no more mem to alloc jpeg_buf buffer, size:%d", rgb888_buf_size);
		csk_free(rgb888_buf);
		return -ENOMEM;
	}
	rgb565_to_rgb888_le(pic, rgb888_buf, w * h);

	/* compress the picture to jpg format */
	struct jpeg_write_context jpeg_write_ctx = {
		.buf = jpeg_buf,
		.total_size = jpeg_buf_size,
		.wrote = 0,
	};
	err = tje_encode_with_func(jpeg_write, &jpeg_write_ctx, 1, w, h, 3, rgb888_buf);
	if (err == 0) {
		LOG_ERR("jpeg compression failed");
        csk_free(rgb888_buf);
        csk_free(jpeg_buf);
		return -EIO;
	} else {
		LOG_INF("jpeg compression done, size:%d/%d", jpeg_write_ctx.wrote, rgb888_buf_size);
	}
	/* rgb888_buf no longer needs to be used */
	csk_free(rgb888_buf);

	// app_obj_rec_ui_evt_notify(APP_OBJ_REC_EVT_UI_ERR_PROCESSING, NULL, 0);

	struct session_objrec_result result = {0};
	session_objrec_t objrec = session_objrec_new();
	err = session_objrec_run(objrec, jpeg_write_ctx.buf, jpeg_write_ctx.wrote, &result, 10 * 1000);
	if (err) {
		LOG_ERR("obj recognition failed, err: %d", err);
	} else {
		// app_obj_rec_ui_evt_notify(APP_OBJ_REC_EVT_TTS_RESULT, result.tts_url, strlen(result.tts_url));
		// app_obj_rec_ui_evt_notify(APP_OBJ_REC_EVT_TEXT_RESULT, result.text_url, strlen(result.tts_url));
		#include "player_manager.h"
		app_player_start(TTS_PLAYER, result.tts_url);
	}
	session_objrec_delete(objrec);

	csk_free(jpeg_buf);

	return err;
}

static int app_obj_rec_obj_recognition_handle(uint32_t id, void *data, uint32_t len)
{
	int err;

	app_obj_rec_run_mode = APP_OBJ_REC_RUN_MODE_OFF;

	if (vbuf_capture) {
		err = app_obj_rec_obj_recognition(vbuf_capture->buffer, CAMERA_WIDTH,
						  CAMERA_HEIGHT);
		if (err) {
			LOG_ERR("app_obj_rec_obj_recognition failed, err: %d", err);
			// char errmsg[] = "发生错误，无法进行识别";
			/* TODO: Error occurred, notify UI */
			// app_obj_rec_ui_evt_notify(APP_OBJ_REC_EVT_UI_ERR_OCCURRED, errmsg, strlen(errmsg));
		}
		app_camera_vbuf_unref(vbuf_capture);
		vbuf_capture = NULL;
	}

	return 0;
}

/**
 * @note thread safety
 */
static int app_obj_rec_new_pic_recv_handle(void *data, uint32_t len)
{

	if (app_obj_rec_run_mode == APP_OBJ_REC_RUN_MODE_PREVIEW) {
		// app_obj_rec_ui_evt_notify(APP_OBJ_REC_EVT_UI_PICTURE_DISPLAY, data, len);
		/* display the picture */
	} else if (app_obj_rec_run_mode == APP_OBJ_REC_RUN_MODE_PICTURE_TAKE) {
	}

	return 0;
}

static void app_camera_evt_handle(uint32_t evt, void *data, uint32_t len)
{
	switch (evt) {
	case APP_CAMERA_EVT_PICTURE:
		app_obj_rec_new_pic_recv_handle(data, len);
		break;
	default:
		break;
	}
}

static int app_obj_rec_enter_handle(uint32_t id, void *data, uint32_t len)
{
	if (app_obj_rec_started) {
		LOG_INF("obj rec is already started, run mode:%d", app_obj_rec_run_mode);
		if (app_obj_rec_run_mode != APP_OBJ_REC_RUN_MODE_PREVIEW) {
			app_obj_rec_run_mode = APP_OBJ_REC_RUN_MODE_PREVIEW;
		}

		if (vbuf_capture) {
			app_camera_vbuf_unref(vbuf_capture);
			vbuf_capture = NULL;
		}

		/*  */
		
		return 0;
	}

	app_obj_rec_run_mode = APP_OBJ_REC_RUN_MODE_PREVIEW;
	app_camera_evt_cb_register(app_camera_evt_handle);
	// camera_start();

	app_obj_rec_started = true;

	return 0;
}

static int app_obj_rec_exit_handle(uint32_t id, void *data, uint32_t len)
{
	if (!app_obj_rec_started) {
		return 0;
	}

	if (vbuf_capture) {
		app_camera_vbuf_unref(vbuf_capture);
		vbuf_capture = NULL;
	}

	app_obj_rec_run_mode = APP_OBJ_REC_RUN_MODE_OFF;
	app_camera_evt_cb_clear();
	// camera_stop();

	app_obj_rec_started = false;

	return 0;
}

static int app_obj_rec_msg_handle(uint32_t id, void *data, uint32_t len)
{
	app_obj_rec_msg_handle_t handle;

	if (id >= APP_OBJ_REC_MSG_ID_MAX) {
		LOG_ERR("Invalid message id: %d", id);
		return -1;
	}

	handle = app_obj_rec_msg_handles[id];
	if (handle == NULL) {
		LOG_ERR("Invalid message handle: %d", id);
		return -2;
	}

	return handle(id, data, len);
}

int app_obj_rec(int argc, char **argv)
{
	return 0;
}

static void app_obj_rec_thread(void *p1, void *p2, void *p3)
{
	int err;
	struct app_obj_rec_msg msg;

	LOG_INF("obj rec thread running");
	
	while (1) {
		err = k_msgq_get(&app_obj_rec_msgq, &msg, K_FOREVER);

		if (err) {
			LOG_ERR("app main msgq get failed, err:%d", err);
			continue;
		}

		LOG_INF("app main msg received, id: %d", msg.id);

		err = app_obj_rec_msg_handle(msg.id, msg.data, msg.len);
		if (err) {
			LOG_ERR("app main msgq handle failed, id:%d, err:%d", msg.id, err);
		}
	}
}

static int app_obj_sys_init(void)
{
	k_thread_stack_t *stack;
	k_tid_t tid;
	const int stack_size = 10 * 1024;
	const int thread_prio = 6;

	stack = csk_aligned_alloc(8,stack_size);
	if (stack == NULL) {
		LOG_ERR("app obj rec stack malloc failed");
		return -ENOMEM;
	}

	struct k_thread *new_thread = csk_malloc(sizeof(struct k_thread));
	if (new_thread == NULL) {
		csk_free(stack);
		return -ENOMEM;
	}

	tid = k_thread_create(new_thread, stack, stack_size, app_obj_rec_thread, NULL, NULL, NULL,
			      thread_prio, 0, K_NO_WAIT);
	if (tid != new_thread) {
		csk_free(new_thread);
		csk_free(stack);
		LOG_ERR("obj rec thread create failed: %p", tid);
		return -EFAULT;
	}

	k_thread_name_set(tid, "objrec");
	LOG_INF("obj rec thread created successfully");

	return 0;
}

SYS_INIT(app_obj_sys_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
