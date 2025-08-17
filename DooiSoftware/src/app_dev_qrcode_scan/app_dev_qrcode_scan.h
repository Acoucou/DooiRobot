#ifndef __APP_DEV_QRCODE_SCAN_H__
#define __APP_DEV_QRCODE_SCAN_H__

#include "stdint.h"

enum {
	DEV_QRCODE_EVT_ENTER = 0,
	DEV_QRCODE_EVT_EXITING,
	DEV_QRCODE_EVT_EXIT,
	DEV_QRCODE_EVT_PICTURE,
	DEV_QRCODE_EVT_SCAN_RESULT,
	DEV_QRCODE_EVT_MAX,
};

enum {
	DEV_QRCODE_UI_EVT_NEW_PICTURE = 0,
	DEV_QRCODE_UI_DEV_CONNECTING,        /* @struct dev_qrcode_scan_result */
	DEV_QRCODE_UI_DEV_CONNECTION_RESULT, /* @struct dev_qrcode_connection_result */
};

struct dev_qrcode_scan_result {
	char pro_id[100];
	char ser_id[100];
};

struct dev_qrcode_connection_result {
	struct dev_qrcode_scan_result *scan_result;
	const char *err_msg;
	int32_t err_code;
};

int app_dev_qrcode_enter(void);
int app_dev_qrcode_exit(void);

void app_dev_qrcode_msg_send(uint32_t id, void *data, uint32_t len, bool free);
void dev_qrcode_ui_evt_notify(uint32_t evt, void *data, uint32_t len);

#endif
