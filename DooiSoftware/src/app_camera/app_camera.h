#ifndef __APP_CAMERA_H__
#define __APP_CAMERA_H__

#include "stdint.h"
#include <zephyr/drivers/video.h>

#define CAMERA_WIDTH  (240)
#define CAMERA_HEIGHT (240)

enum {
	APP_CAMERA_EVT_NONE = 0,
	APP_CAMERA_EVT_PICTURE,
};

typedef void (*app_camera_evt_callback_t)(uint32_t evt, void *data, uint32_t len);
void app_camera_evt_cb_register(app_camera_evt_callback_t cb);
void app_camera_evt_cb_clear(void);
void camera_start(void);
void camera_stop(void);
void app_camera_flip();

void app_camera_vbuf_unref(struct video_buffer *vbuf);
int app_camera_vbuf_capture(struct video_buffer **vbuf, int *w, int *h);

#endif
