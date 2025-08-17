/*
 * Copyright (c) 2023 Anhui Listenai Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/video.h>
#include <drivers/csk6_video_extend.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(camera_logger, 4);

#include "app_camera.h"

#define CAMERA_NODE     DT_NODELABEL(dvp)
#define CAMERA_VBUF_NUM (CONFIG_VIDEO_BUFFER_POOL_NUM_MAX)

#define CAMERA_THREAD_STACK_SIZE KB(2)
#define CAMERA_THREAD_PRIORITY   6

K_EVENT_DEFINE(camera_run);

static const struct device *camera = DEVICE_DT_GET(CAMERA_NODE);
static app_camera_evt_callback_t evt_cb = NULL;

void app_camera_flip()
{
	static uint8_t back = true;
	int ret;

	back = !back;
	LOG_INF("switch to back mode: %d", back);

	if (back) {
		int value = true;
		ret = video_set_ctrl(camera, VIDEO_CID_HFLIP, &value);
		if (ret) {
			LOG_ERR("Failed to set camera hflip control, err: %d", ret);
		}
		value = true;
		ret = video_set_ctrl(camera, VIDEO_CID_VFLIP, &value);
		if (ret) {
			LOG_ERR("Failed to set camera vflip control, err: %d", ret);
		}
	} else {
		int value = true;
		ret = video_set_ctrl(camera, VIDEO_CID_HFLIP, &value);
		if (ret) {
			LOG_ERR("Failed to set camera hflip control, err: %d", ret);
		}
		value = false;
		ret = video_set_ctrl(camera, VIDEO_CID_VFLIP, &value);
		if (ret) {
			LOG_ERR("Failed to set camera vflip control, err: %d", ret);
		}
	}
}

static void app_camera_evt_notify(uint32_t evt, void *data, uint32_t len)
{
	if (evt_cb) {
		evt_cb(evt, data, len);
	}
}

void app_camera_evt_cb_register(app_camera_evt_callback_t cb)
{
	evt_cb = cb;
}

void app_camera_evt_cb_clear(void)
{
	evt_cb = NULL;
}

int camera_init(void)
{
	struct video_caps caps;
	struct video_format fmt;
	struct video_buffer *buffers[CAMERA_VBUF_NUM];
	int ret;

	if (!device_is_ready(camera)) {
		LOG_ERR("Camera not ready: %s", camera->name);
		return -ENODEV;
	}

	LOG_DBG("Camera %s ready", camera->name);

	ret = video_get_caps(camera, VIDEO_EP_OUT, &caps);
	if (ret < 0) {
		LOG_ERR("Failed to get caps: %d", ret);
		return ret;
	}

	const struct video_format_cap *cap = caps.format_caps;
	for (uint32_t i = 0; cap->pixelformat != 0; cap = &caps.format_caps[++i]) {
		LOG_DBG("* format: %c%c%c%c, width: [%u-%u, %u], height: [%u-%u, %u]",
			cap->pixelformat & 0xff, (cap->pixelformat >> 8) & 0xff,
			(cap->pixelformat >> 16) & 0xff, (cap->pixelformat >> 24) & 0xff,
			cap->width_min, cap->width_max, cap->width_step, cap->height_min,
			cap->height_max, cap->height_step);
	}

	fmt.pixelformat = VIDEO_PIX_FMT_RGB565;
	fmt.width = CAMERA_WIDTH;
	fmt.height = CAMERA_HEIGHT;
	fmt.pitch = fmt.width * 2;
	ret = video_set_format(camera, VIDEO_EP_OUT, &fmt);
	if (ret < 0) {
		LOG_ERR("Failed to set format: %d", ret);
		return ret;
	}

	for (uint32_t i = 0; i < CAMERA_VBUF_NUM; i++) {
		buffers[i] = video_buffer_alloc(fmt.pitch * fmt.height);
		if (!buffers[i]) {
			LOG_ERR("Failed to alloc buffer %d", i);
			return -ENOMEM;
		}
		video_enqueue(camera, VIDEO_EP_OUT, buffers[i]);
	}

	int value = true;
	ret = video_set_ctrl(camera, VIDEO_CID_VFLIP, &value);
	if (ret) {
		LOG_ERR("Failed to set camera vflip control, err: %d", ret);
	}
	value = false;
	ret = video_set_ctrl(camera, VIDEO_CID_HFLIP, &value);
	if (ret) {
		LOG_ERR("Failed to set camera hflip control, err: %d", ret);
	}

	return 0;
}

void camera_start(void)
{
	video_stream_start(camera);
	k_event_set(&camera_run, BIT(0));
}

void camera_stop(void)
{
	struct video_buffer *vbuf;
	k_event_clear(&camera_run, BIT(0));
	video_stream_stop(camera);

	while (video_dequeue(camera, VIDEO_EP_OUT, &vbuf, K_NO_WAIT) == 0) {
		video_enqueue(camera, VIDEO_EP_OUT, vbuf);
	}
}

K_MUTEX_DEFINE(app_camera_lock);

// static void camera_thread(void *p1, void *p2, void *p3)
// {
// 	struct video_buffer *vbuf;
// 	int ret;

// 	ret = camera_init();
// 	if (ret) {
// 		LOG_ERR("Failed to initialize camera: %d", ret);
// 		return;
// 	}

// 	while (1) {
// 		k_mutex_lock(&app_camera_lock, K_FOREVER);
// 		ret = video_dequeue(camera, VIDEO_EP_OUT, &vbuf, K_FOREVER);
// 		if (ret < 0) {
// 			LOG_ERR("Failed to dequeue buffer: %d", ret);
// 			continue;
// 		}
// 		k_mutex_unlock(&app_camera_lock);
// 		app_camera_evt_notify(APP_CAMERA_EVT_PICTURE, vbuf->buffer, vbuf->bytesused);
// 		video_enqueue(camera, VIDEO_EP_OUT, vbuf);
// 		k_msleep(10000);
// 	}
// }

int app_camera_vbuf_capture(struct video_buffer **vbuf, int *w, int *h)
{
	int err;
	*vbuf = NULL;
	k_mutex_lock(&app_camera_lock, K_FOREVER);
	err = video_dequeue(camera, VIDEO_EP_OUT, vbuf, K_FOREVER);
	k_mutex_unlock(&app_camera_lock);

	return err;
}

void app_camera_vbuf_unref(struct video_buffer *vbuf)
{
	video_enqueue(camera, VIDEO_EP_OUT, vbuf);
}

// K_THREAD_DEFINE(camera_thread_id, CAMERA_THREAD_STACK_SIZE, camera_thread, NULL, NULL, NULL,
// 		CAMERA_THREAD_PRIORITY, 0, 0);