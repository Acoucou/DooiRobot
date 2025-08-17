#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(dev_qrcode, LOG_LEVEL_DBG);

#include <qrcode_gcl.h>
#include "csk_malloc.h"
#include "app_camera.h"
#include "../app_wifi_qrcode_scan/ls_qrcode_types.h"
#include "app_dev_qrcode_scan.h"

struct dev_qrcode_msg_data {
	uint32_t evt;
	void *data;
	uint32_t data_len;
	bool free;
};

K_SEM_DEFINE(dev_qrcode_engine_done_sem, 0, 1);
K_SEM_DEFINE(dev_qrcode_result_done_sem, 0, 1);
K_SEM_DEFINE(dev_qrcode_exit_done_sem, 0, 1);
K_SEM_DEFINE(dev_qrcode_scan_done_sem, 0, 1);
K_MSGQ_DEFINE(dev_qrcode_msgq, sizeof(struct dev_qrcode_msg_data), 20, 4);

enum {
	DEV_QRCODE_RUN_MODE_NOT_INIT = 0,
	DEV_QRCODE_RUN_MODE_INITED,
	DEV_QRCODE_RUN_MODE_DETECTING,
	DEV_QRCODE_RUN_MODE_CONNECT,
	DEV_QRCODE_RUN_MODE_STOPPING,
	DEV_QRCODE_RUN_MODE_IDLE,
};

static uint8_t dev_qrcode_run_mode = DEV_QRCODE_RUN_MODE_NOT_INIT;
static uint8_t *detecting_picture = NULL;
static const int detecting_picture_size = 3 * CAMERA_WIDTH * CAMERA_HEIGHT;

typedef struct {
	uint32_t rlt_type;
	uint32_t detect_cnt;
	ls_qrcode_detect detect_rlt[0];
} __attribute__((packed)) alg_rlt_t;

typedef struct {
	uint16_t fmt;
	uint16_t width;
	uint16_t height;
	uint32_t len;
	uint8_t *data;
} __attribute__((packed)) img_frame_t;

typedef int (*dev_qrcode_msg_handler_t)(void *data, uint32_t len);

static void app_camera_evt_callback(uint32_t evt, void *data, uint32_t len);

static int dev_qrcode_enter_handle(void *data, uint32_t len);
static int dev_qrcode_exiting_handle(void *data, uint32_t len);
static int dev_qrcode_exit_handle(void *data, uint32_t len);
static int dev_qrcode_result_handle(void *data, uint32_t len);
static int dev_qrcode_picture_handle(void *data, uint32_t len);

static dev_qrcode_msg_handler_t dev_qrcode_msg_handles[DEV_QRCODE_EVT_MAX] = {
		[DEV_QRCODE_EVT_ENTER] = dev_qrcode_enter_handle,
		[DEV_QRCODE_EVT_EXITING] = dev_qrcode_exiting_handle,
		[DEV_QRCODE_EVT_EXIT] = dev_qrcode_exit_handle,
		[DEV_QRCODE_EVT_PICTURE] = dev_qrcode_picture_handle,
		[DEV_QRCODE_EVT_SCAN_RESULT] = dev_qrcode_result_handle,
};

static void dev_qrcode_scan_gcl_event_cb(uint32_t event, void *event_data, uint32_t event_data_len, void *priv)
{
	int err;

	LOG_DBG("qrcode gcl evt:%d", event);

	if (QRCODE_CB_EVENT_ENGINE_STARTED == event) {

	} else if (QRCODE_CB_EVENT_ENGINE_STOPED == event) {
		k_sem_give(&dev_qrcode_engine_done_sem);
	} else if (QRCODE_CB_EVENT_ENGINE_STREAM_EVENT == event) {

	} else if (QRCODE_CB_EVENT_ALGO_RLT == event) {
		alg_rlt_t *alg_rlt = event_data;
		LOG_INF("result type:%d, cnt:%d", alg_rlt->rlt_type, alg_rlt->detect_cnt);

		int len = sizeof(alg_rlt_t) + sizeof(ls_qrcode_detect) * alg_rlt->detect_cnt;
		alg_rlt_t *alg_rlt_tmp = csk_malloc(len);
		if (alg_rlt_tmp == NULL) {
			LOG_ERR("qrcode gcl cb, buffer malloc failed, size:%d", len);
			return;
		}

		memcpy(alg_rlt_tmp, ((uint8_t *)alg_rlt), len);
		LOG_DBG("qrcode gcl cb, malloc addr:%p", alg_rlt_tmp);
		struct dev_qrcode_msg_data msg = {
				.data = alg_rlt_tmp,
				.data_len = len,
				.free = true,
				.evt = DEV_QRCODE_EVT_SCAN_RESULT,
		};

		err = k_msgq_put(&dev_qrcode_msgq, &msg, K_NO_WAIT);
		if (err) {
			LOG_ERR("msgq put err, ret:%d", err);
			csk_free(alg_rlt_tmp);
		}
	}
}

static int dev_qrcode_enter_handle(void *data, uint32_t len)
{
	int err;

	qrcode_gcl_params_t params = {
			.qrcode_thres = 0.45,
			.qrcode_pixesize = 20,
			.qrcode_probthres = 0.35,
			.qrcode_nsmthres = 0.35,
	};

	if (dev_qrcode_run_mode != DEV_QRCODE_RUN_MODE_NOT_INIT) {
		LOG_ERR("dev qrcode enter failed");
		return -ENOTSUP;
	}

	err = qrcode_gcl_init();
	if (err) {
		LOG_ERR("qrcode gcl init failed, err:%d", err);
		return err;
	}

	err = qrcode_gcl_params_set(&params);
	if (err) {
		LOG_ERR("qrcode gcl paras set failed, err:%d", err);
		return err;
	}

	err = qrcode_gcl_add_callback(QRCODE_CB_EVENT_ENGINE_STARTED | QRCODE_CB_EVENT_ENGINE_STOPED |
										  QRCODE_CB_EVENT_ENGINE_STREAM_EVENT | QRCODE_CB_EVENT_ALGO_RLT,
			dev_qrcode_scan_gcl_event_cb, NULL);
	if (err) {
		LOG_ERR("qrcode gcl cb add failed, err:%d", err);
		return err;
	}

	err = qrcode_gcl_prepare();
	if (err) {
		LOG_ERR("qrcode gcl prepare failed, err:%d", err);
		qrcode_gcl_deinit();
		return err;
	}

	err = qrcode_gcl_engine_start();
	if (err) {
		LOG_ERR("qrcode gcl engine start failed, err:%d", err);
		return err;
	}

	/* BGR888 */
	detecting_picture = csk_malloc(detecting_picture_size);
	if (detecting_picture == NULL) {
		LOG_ERR("dev qrcode detecting picture malloc failed, err:%d", err);
		return -ENOMEM;
	}

	app_camera_evt_cb_register(app_camera_evt_callback);

	camera_start();

	dev_qrcode_run_mode = DEV_QRCODE_RUN_MODE_INITED;

	return 0;
}

static int dev_qrcode_exiting_handle(void *data, uint32_t len)
{
	if (dev_qrcode_run_mode == DEV_QRCODE_RUN_MODE_NOT_INIT) {
		k_sem_give(&dev_qrcode_exit_done_sem);
		return 0;
	}

	if (dev_qrcode_run_mode != DEV_QRCODE_RUN_MODE_STOPPING) {
		dev_qrcode_run_mode = DEV_QRCODE_RUN_MODE_STOPPING;
	}

	app_dev_qrcode_msg_send(DEV_QRCODE_EVT_EXIT, NULL, 0, false);

	return 0;
}

static int dev_qrcode_exit_handle(void *data, uint32_t len)
{
	static uint8_t retry_count = 0;
	int err;

	err = k_sem_take(&dev_qrcode_result_done_sem, K_NO_WAIT);

	if (err) {
		if (retry_count >= 5) {
			LOG_ERR("qrcode exit retry timeout");
			retry_count = 0;
		} else {
			app_dev_qrcode_msg_send(DEV_QRCODE_EVT_EXIT, NULL, 0, false);
			k_msleep(200);
			retry_count++;
			LOG_WRN("qrcode state is node idle, retry count:%d", retry_count);
		}
	}

	qrcode_gcl_remove_callback(dev_qrcode_scan_gcl_event_cb);
	err = qrcode_gcl_engine_stop();
	if (err) {
		LOG_ERR("dev qrcode gcl engine stop failed: %d", err);
		return err;
	}

	/* wait gcl engine stop done */
	err = k_sem_take(&dev_qrcode_engine_done_sem, K_MSEC(10 * 1000));
	if (err) {
		LOG_ERR("dev qrcode wait gcl engine stop failed: %d", err);
		return err;
	}

	err = qrcode_gcl_deinit();
	if (err) {
		LOG_ERR("dev qrcode gcl deinit failed: %d", err);
		return err;
	}

	app_camera_evt_cb_clear();
	camera_stop();

	if (detecting_picture) {
		csk_free(detecting_picture);
		detecting_picture = NULL;
	}

	dev_qrcode_run_mode = DEV_QRCODE_RUN_MODE_NOT_INIT;

	struct dev_qrcode_msg_data msg = {
			.data = NULL,
			.data_len = 0,
			.evt = DEV_QRCODE_EVT_PICTURE,
			.free = false,
	};

	while (k_msgq_get(&dev_qrcode_msgq, &msg, K_NO_WAIT) == 0) {
		if (msg.free) {
			csk_free(msg.data);
		}
	}

	k_sem_give(&dev_qrcode_exit_done_sem);
	LOG_INF("dev qrcode exit done");
	retry_count = 0;

	return 0;
}

static void app_camera_evt_callback(uint32_t evt, void *data, uint32_t len)
{
	LOG_DBG("dev qrcode camera evt:%d", evt);

	if (evt == APP_CAMERA_EVT_PICTURE) {
		// dev_qrcode_ui_evt_notify(DEV_QRCODE_UI_EVT_NEW_PICTURE, data, len);
		struct dev_qrcode_msg_data msg = {
				.data = NULL,
				.data_len = 0,
				.evt = DEV_QRCODE_EVT_PICTURE,
				.free = false,
		};

		int err = k_msgq_put(&dev_qrcode_msgq, &msg, K_FOREVER);
		if (err) {
			LOG_ERR("camera picture msg put failed, err:%d", err);
		}
	}
}

int parse_qrcode_result(const char *result, struct dev_qrcode_scan_result *dev_result)
{
    char *product_id;
    char *secret_id;
    char *token;

    // 使用 strtok 函数按照 & 分隔字符串
    token = strtok((char *)result, "&");
    if (token) {
        product_id = token;
        token = strtok(NULL, "&");
        if (token) {
            secret_id = token;
            // 存储到全局变量
            strcpy(dev_result->pro_id, product_id);
            strcpy(dev_result->ser_id, secret_id);
            return 0;
        } else {
            return 5;
        }
    }
    return 0;
}
static const char *dev_qrcode_err_code_to_str_zh(int err)
{
	switch (err) {
		case 0:
			return "扫描成功";
		case 2:
			return "未知错误";
		case 3: /* pass */
		case 4: /* pass */
		case 5:
			return "dev不存在";
		default:
			break;
	}

	return "未知错误";
}

static int dev_qrcode_recognize_result_proc(char *payload)
{
	int err;

	struct dev_qrcode_scan_result *scan_result = csk_malloc(sizeof(struct dev_qrcode_scan_result));
	if (scan_result == NULL) {
		LOG_ERR("qrcode scan result malloc failed");
		return -ENOMEM;
	}

	err = parse_qrcode_result(payload, scan_result);
	if (err) {
		LOG_ERR("qrcode scan result parse failed, err:%d", err);
		csk_free(scan_result);
		return err;
	}

	// dev_qrcode_ui_evt_notify(DEV_QRCODE_UI_DEV_CONNECTING, scan_result, sizeof(struct dev_qrcode_scan_result));
    // extern void set_dev_text(char *pro,char *sec);
    // set_dev_text(scan_result->pro_id, scan_result->ser_id);

	const char *err_msg = dev_qrcode_err_code_to_str_zh(err);
	struct dev_qrcode_connection_result conn_result = {
			.scan_result = scan_result,
			.err_code = err,
			.err_msg = err_msg,
	};

	// dev_qrcode_ui_evt_notify(
	// 		DEV_QRCODE_UI_DEV_CONNECTION_RESULT, &conn_result, sizeof(struct dev_qrcode_connection_result));

	csk_free(scan_result);

	return 0;
}

static int dev_qrcode_result_handle(void *data, uint32_t len)
{
	alg_rlt_t *result = data;
	int err;

	if (dev_qrcode_run_mode == DEV_QRCODE_RUN_MODE_NOT_INIT) {
		return -ENOTSUP;
	}

	LOG_DBG("qrcode result type:%d, cnt:%d", result->rlt_type, result->detect_cnt);

	if (result->rlt_type == QRCODE_ENGINE_DETECT_RLT) {
		if (result->detect_cnt) {
			err = qrcode_gcl_engine_recognize();
			if (err) {
				LOG_ERR("qrcode engine rec failed, err:%d", err);
			}
		} else {
			dev_qrcode_run_mode = DEV_QRCODE_RUN_MODE_IDLE;
			k_sem_give(&dev_qrcode_result_done_sem);
		}
	} else if (result->rlt_type == QRCODE_ENGINE_RECOGNIZE_RLT) {
		dev_qrcode_run_mode = DEV_QRCODE_RUN_MODE_IDLE;
		k_sem_give(&dev_qrcode_result_done_sem);
		if (result->detect_cnt) {
			char *text = result->detect_rlt[0].qrcode_result.payload;
			if (strlen(text)) {
				LOG_INF("qrcode scan result:%s", text);

				LOG_INF("dev qrcode auto exit");
				dev_qrcode_run_mode = DEV_QRCODE_RUN_MODE_STOPPING;
				err = dev_qrcode_exit_handle(NULL, 0);
				if (err) {
					LOG_ERR("dev qrcode wait exited failed: %d", err);
				} else {
					LOG_INF("dev qrcode exited successfully");
				}
				err = dev_qrcode_recognize_result_proc(text);
				if (err) {
					LOG_ERR("dev_qrcode_recognize_result_proc failed: %d", err);
				}
			}
		}
	}

	return 0;
}

static void rgb565_to_bgr888_le(uint16_t *src, uint8_t *dst, size_t pixel_count)
{
	for (size_t i = 0; i < pixel_count; i++) {
		uint16_t pixel = __builtin_bswap16(src[i]);
		uint8_t r = (pixel >> 11) & 0x1F;
		uint8_t g = (pixel >> 5) & 0x3F;
		uint8_t b = pixel & 0x1F;

		dst[i * 3 + 0] = (b * 255) / 31; // Blue
		dst[i * 3 + 1] = (g * 255) / 63; // Green
		dst[i * 3 + 2] = (r * 255) / 31; // Red
	}
}

static int dev_qrcode_picture_handle(void *data, uint32_t len)
{
	int err = 0;

	if (dev_qrcode_run_mode == DEV_QRCODE_RUN_MODE_NOT_INIT) {
		return -ENOTSUP;
	}

	if (dev_qrcode_run_mode == DEV_QRCODE_RUN_MODE_DETECTING) {
		return 0;
	}

	if (dev_qrcode_run_mode == DEV_QRCODE_RUN_MODE_STOPPING) {
		return 0;
	}

	struct video_buffer *vbuf = NULL;
	int w;
	int h;
	err = app_camera_vbuf_capture(&vbuf, &w, &h);
	if (err) {
		return err;
	}

	img_frame_t img = {
			.fmt = LS_PIX_FMT_BGR888,
			.width = CAMERA_WIDTH,
			.height = CAMERA_HEIGHT,
			.data = detecting_picture,
			.len = detecting_picture_size,
	};

	rgb565_to_bgr888_le((uint16_t *)vbuf->buffer, detecting_picture, CAMERA_WIDTH * CAMERA_HEIGHT);
	app_camera_vbuf_unref(vbuf);

	LOG_DBG("start to qrcode gcl stream write");

	k_sem_reset(&dev_qrcode_result_done_sem);

	err = qrcode_gcl_stream_write((uint8_t *)&img, sizeof(img));
	if (err == 0) {
		dev_qrcode_run_mode = DEV_QRCODE_RUN_MODE_DETECTING;
	} else {
		LOG_ERR("qrcode stream write failed, err:%d", err);
	}

	return err;
}

static int dev_qrcode_msg_handle(struct dev_qrcode_msg_data *msg)
{
	int err;

	if (msg->evt >= DEV_QRCODE_EVT_MAX) {
		LOG_ERR("dev qrcode invalid message, id:%d", msg->evt);
		return -EINVAL;
	}

	dev_qrcode_msg_handler_t handle = dev_qrcode_msg_handles[msg->evt];
	if (handle != NULL) {
		err = handle(msg->data, msg->data_len);
	} else {
		LOG_ERR("dev qrcode msg handler is NULL, id:%d", msg->evt);
		return -EINVAL;
	}

	return 0;
}

static void dev_qrcode_thread(void *p1, void *p2, void *p3)
{
	int err;
	struct dev_qrcode_msg_data msg;

	while (1) {
		err = k_msgq_get(&dev_qrcode_msgq, &msg, K_FOREVER);
		if (err) {
			LOG_ERR("dev qrcode get msg failed, err:%d", err);
			continue;
		}

		LOG_DBG("dev qrcode evt:%d", msg.evt);

		err = dev_qrcode_msg_handle(&msg);
		if (err) {
			LOG_ERR("dev qrcode msg handle failed, err: %d", err);
		}

		LOG_DBG("dev qrcode msg id:%d handle done, free:%d, err:%d", msg.evt, msg.free, err);
		if (msg.free) {
			csk_free(msg.data);
		}
	}
}

static int dev_qrcode_sys_init(void)
{
	k_thread_stack_t *stack;
	k_tid_t tid;
	const int stack_size = 4 * 1024;
	const int thread_prio = 6;

	stack = csk_aligned_alloc(8, stack_size);
	if (stack == NULL) {
		LOG_ERR("dev qrcode stack malloc failed");
		return -ENOMEM;
	}

	struct k_thread *new_thread = csk_malloc(sizeof(struct k_thread));
	if (new_thread == NULL) {
		csk_free(stack);
		return -ENOMEM;
	}

	tid = k_thread_create(
			new_thread, stack, stack_size, dev_qrcode_thread, NULL, NULL, NULL, thread_prio, 0, K_NO_WAIT);
	if (tid != new_thread) {
		csk_free(new_thread);
		csk_free(stack);
		LOG_ERR("obj rec thread create failed: %p", tid);
		return -EFAULT;
	}

	k_thread_name_set(tid, "dev_qrcode");

	return 0;
}

void app_dev_qrcode_msg_send(uint32_t id, void *data, uint32_t len, bool free)
{
	int err;
	struct dev_qrcode_msg_data msg = {
			.evt = id,
			.data = data,
			.data_len = len,
			.free = free,
	};

	err = k_msgq_put(&dev_qrcode_msgq, &msg, K_FOREVER);
	if (err) {
		LOG_ERR("app main msg send failed, err:%d, id:%d", err, id);
	}
}

int app_dev_qrcode_enter(void)
{
	app_dev_qrcode_msg_send(DEV_QRCODE_EVT_ENTER, NULL, 0, false);

	return 0;
}

int app_dev_qrcode_exit(void)
{
	int err;

	k_sem_reset(&dev_qrcode_exit_done_sem);
	app_dev_qrcode_msg_send(DEV_QRCODE_EVT_EXITING, NULL, 0, false);
	err = k_sem_take(&dev_qrcode_exit_done_sem, K_MSEC(10 * 1000));

	if (err) {
		LOG_ERR("dev qrcode wait exited failed: %d", err);
	} else {
		LOG_INF("dev qrcode exited successfully");
	}

	return err;
}

SYS_INIT(dev_qrcode_sys_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
