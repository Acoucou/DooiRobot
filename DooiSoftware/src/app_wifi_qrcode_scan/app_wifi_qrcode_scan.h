#ifndef __APP_WIFI_QRCODE_SCAN_H__
#define __APP_WIFI_QRCODE_SCAN_H__

#include "stdint.h"

enum {
	WIFI_QRCODE_EVT_ENTER = 0,
	WIFI_QRCODE_EVT_EXITING,
	WIFI_QRCODE_EVT_EXIT,
	WIFI_QRCODE_EVT_PICTURE,
	WIFI_QRCODE_EVT_SCAN_RESULT,
	WIFI_QRCODE_EVT_MAX,
};

enum {
	WIFI_QRCODE_UI_EVT_NEW_PICTURE = 0,
	WIFI_QRCODE_UI_WIFI_CONNECTING,        /* @struct wifi_qrcode_scan_result */
	WIFI_QRCODE_UI_WIFI_CONNECTION_RESULT, /* @struct wifi_qrcode_connection_result */
};

#define WIFI_SSID_MAX_LEN (64)
#define WIFI_PWD_MAX_LEN  (64)

struct wifi_qrcode_scan_result {
	char ssid[WIFI_SSID_MAX_LEN];
	char password[WIFI_PWD_MAX_LEN];
	char auth[WIFI_PWD_MAX_LEN];
};

struct wifi_qrcode_connection_result {
	struct wifi_qrcode_scan_result *scan_result;
	const char *err_msg;
	int32_t err_code;
};

int app_wifi_qrcode_enter(void);
int app_wifi_qrcode_exit(void);

void app_wifi_qrcode_msg_send(uint32_t id, void *data, uint32_t len, bool free);
void wifi_qrcode_ui_evt_notify(uint32_t evt, void *data, uint32_t len);

#endif
