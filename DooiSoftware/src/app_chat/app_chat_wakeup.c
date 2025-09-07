#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "gcs_wakeup_service.h"
#include "player_manager.h"
#include "app_chat.h"
#include "lsc_session_voice.h"
// #include "aweui_event.h"
// #include "app_chat_ui.h"
#include "app_connect.h"
#include "app_chat_session.h"
#if (CONFIG_WAKEUP_DEBUG)
#include "wakeup_debug.h"
#endif
#include "screen.h"

LOG_MODULE_REGISTER(app_chat_wakeup);

#define SKIP_INVALID_AUDIOFRAME_MS (220) // 这是一个实际测量值，为解决唤醒词上传到云端问题

static struct k_timer skip_invalid_audioframe_timer;
static volatile bool if_skip_invalid_audioframe = false;
static volatile bool if_button_wakeup = false;
static volatile bool if_wakeup_start = false;

void app_chat_button_wakeup_enable(bool en)
{
	if_button_wakeup = en;
}

void wakeup_stream_data_process(uint8_t *data, size_t len)
{
	if (data == NULL || len == 0) {
		return;
	}
#if (CONFIG_WAKEUP_DEBUG)
	if (wakeup_service_gcs_get_debug_mode()) {
		uint32_t pos = 0;
		// uint16_t *ptr = (uint16_t *)data;
		usb_send_data(data, len);
		for (int i = 0; i < len / 10; i++) {
			data[pos++] = data[i * 10 + 8];
			data[pos++] = data[i * 10 + 9];
		}
		len = len / 5;
	}
#endif
	if ((if_skip_invalid_audioframe || if_button_wakeup) && (app_connect_get_status() & APP_CONNECT_STATUS_CONNECTED)) {
		app_chat_session_send_audio(data, len);
	}
}

static void notify_wakeup_with_asr(void)
{
	bool started = true;
	app_chat_evt_notify(APP_CHAT_EVT_WAKEUP_ASR, &started, sizeof(bool));
}

static void wakeup_rlt_process(uint8_t *data, size_t len)
{
	// aweui_wakeup_option_t opt = WEUI_WAKEUP_OPTION_NONE;
	// LOG_INF("-------wakeup_rlt: %s\n", data);

	screen_set_current(SCREEN_WAKEUP_ACK);

	// get_wakeup_mode_from_ui(&opt);
	// if (opt != AWEUI_WAKEUP_OPTION_BUTTON) {

		if (app_connect_get_status() & APP_CONNECT_STATUS_DISCONNECTED) {
			if (app_connect_get_status() & APP_CONNECT_STATUS_AUTH_FAILD) {
				app_player_start(TONE_PLAYER, "/lfs/cloud_auth_fail.mp3");
			} else {
				app_player_start(TONE_PLAYER, "/lfs/064_net_not_connected.mp3");
				// extern int ble_start_adv(void);
				// ble_start_adv();
			}
			return;
		}

		if_skip_invalid_audioframe = false;
		notify_wakeup_with_asr();
		k_timer_stop(&skip_invalid_audioframe_timer);
		k_timer_start(&skip_invalid_audioframe_timer, K_MSEC(SKIP_INVALID_AUDIOFRAME_MS), K_NO_WAIT);
	// } else {
	// 	LOG_INF("is in btn wakeup");
	// }
}

static void skip_invalid_audioframe_timer_timeout_process(struct k_timer *timer)
{
	if_skip_invalid_audioframe = true;
}

int app_chat_wakeup_start(void)
{
	if (if_wakeup_start) {
		return 0;
	}
	int ret = wakeup_service_gcs_start();
	if (ret != 0) {
		LOG_WRN("[%s] wakeup_service_gcs_start faild(%d)", __FUNCTION__, ret);
		return -1;
	}

	if_wakeup_start = true;
	return 0;
}

int app_chat_wakeup_exit(void)
{
	int ret = wakeup_service_gcs_stop();
	if (ret != 0) {
		LOG_WRN("[%s] wakeup_service_gcs_stop faild(%d)", __FUNCTION__, ret);
		return -1;
	}

	if_wakeup_start = false;

	return 0;
}

int app_chat_wakeup_init(void)
{
	int ret = 0;

	wakeup_service_gcs_params_t wk_params;
	wakeup_service_gcs_params_get(&wk_params);
	wk_params.ref_gain_val = 1;
	wk_params.mic_gain_a_val = 25;
	wk_params.mic_gain_d_val = 0;
	wakeup_service_gcs_params_set(&wk_params);

	ret = wakeup_service_gcs_init();
	if (ret != 0) {
		// 算法鉴权失败，直接退出应用
		app_player_start(TONE_PLAYER, "/lfs/algo_auth_fail.mp3");
		return -1;
	}
	wakeup_service_gcs_stream_data_callback(wakeup_stream_data_process);
	wakeup_service_gcs_rlt_callback(wakeup_rlt_process);

	k_timer_init(&skip_invalid_audioframe_timer, skip_invalid_audioframe_timer_timeout_process, NULL);

	app_chat_wakeup_start();

	return 0;
}