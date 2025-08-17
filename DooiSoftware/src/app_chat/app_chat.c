#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "player_manager.h"
#include "lsc_session_voice.h"
#include "lsc_errno.h"
#include "app_chat.h"
// #include "aweui_event.h"
#include "lsc_stream_text.h"
#include "csk_malloc.h"
#include "app_connect.h"
#include "app_chat_session.h"
// #include "app_chat_key.h"
#include "app_chat_wakeup.h"
// #include "app_chat_ui.h"
#include "app_download.h"
#include "app_music_ctrl.h"

#ifdef CONFIG_UART_DISP_EN
#include "uart_disp.h"
#endif

LOG_MODULE_REGISTER(app_chat);

#define ASR_REULST_DISPLAY_TIME_MS (5000)

K_MSGQ_DEFINE(iat_result_msgq, sizeof(app_chat_evt_msg_t), 10, 4);
K_MSGQ_DEFINE(nlp_consumer_msgq, sizeof(app_chat_evt_msg_t), 10, 4);
K_MSGQ_DEFINE(aiui_result_msgq, sizeof(app_chat_evt_msg_t), 10, 4);
K_MSGQ_DEFINE(tts_result_msgq, sizeof(app_chat_evt_msg_t), 10, 4);
K_MSGQ_DEFINE(wakeup_msgq, sizeof(app_chat_evt_msg_t), 10, 4);

static volatile bool is_in_session = false;
// static struct k_work asr_result_work;
static struct k_timer asr_result_timer;
struct lsc_stream_text_request_ctx *text_req_ctx = NULL;

static void asr_result_timer_timeout_process(struct k_timer *timer)
{
	// k_work_submit(&asr_result_work);
}

static void asr_result_work_handler(struct k_work *work)
{
	// if (if_app_chat_session_is_continue()) {
	// 	if (is_in_session == true) {
	// 		aweui_set_user_asr_result_text(AWEUI_LLM_STATUS_WAITING, NULL, AWEUI_TEXT_MODE_NONE);
	// 	}
	// } else {
	// 	aweui_set_user_asr_result_text(AWEUI_LLM_STATUS_CLOSE, NULL, AWEUI_TEXT_MODE_NONE);
	// }
}

static void consumer_thread(void *p1, void *p2, void *p3)
{
	int ret = 0;

	app_chat_evt_msg_t msg;
	while (1) {
		ret = k_msgq_get(&aiui_result_msgq, &msg, K_FOREVER);
		if (ret != 0) {
			continue;
		}

		if (msg.event == APP_CHAT_EVT_FINISH) {
		} else if (msg.event == APP_CHAT_EVT_ERR) {
			app_player_start(TONE_PLAYER, "/lfs/065_net_error.mp3");
		}
		app_player_unlock();
		if (if_app_chat_session_is_continue()) {
			k_timer_stop(&asr_result_timer);
			// aweui_set_user_asr_result_text(AWEUI_LLM_STATUS_CLOSE, NULL, AWEUI_TEXT_MODE_NONE);
		} else {
			if (k_timer_remaining_get(&asr_result_timer) == 0 && is_in_session == true) {
				// aweui_set_user_asr_result_text(AWEUI_LLM_STATUS_CLOSE, NULL, AWEUI_TEXT_MODE_NONE);
			}
		}

		if (msg.data != NULL) {
			csk_free(msg.data);
		}
	}
}

void set_timer_close(void)
{
	if (if_app_chat_session_is_continue()) {
		k_timer_stop(&asr_result_timer);
	}
}

static void consumer_iat_thread(void *p1, void *p2, void *p3)
{
	int ret = 0;

	app_chat_evt_msg_t msg;
	while (1) {
		ret = k_msgq_get(&iat_result_msgq, &msg, K_FOREVER);
		if (ret != 0) {
			continue;
		}

		if (msg.event == APP_CHAT_EVT_IAT) {
			LOG_INF("[%s] get iat %s", __FUNCTION__, (char *)msg.data);
			if (is_in_session == true) {
				// aweui_set_user_asr_result_text(
				// 		AWEUI_LLM_STATUS_SUCCESS, (const char *)msg.data, AWEUI_TEXT_MODE_OVERWRITE);
				extern void set_wakeup_ack_text(char * text);
				set_wakeup_ack_text(msg.data);
			}
			k_timer_stop(&asr_result_timer);
			k_timer_start(&asr_result_timer, K_MSEC(ASR_REULST_DISPLAY_TIME_MS), K_NO_WAIT);
		} else {
			LOG_WRN("[%s] unknow event[%d]", __FUNCTION__, msg.event);
		}

		if (msg.data != NULL) {
			csk_free(msg.data);
		}
	}
}

static void wakeup_inter_thread(void *p1, void *p2, void *p3)
{
	app_chat_evt_msg_t msg;
	int ret;
	while (1) {
		ret = k_msgq_get(&wakeup_msgq, &msg, K_FOREVER);
		if (ret != 0) {
			continue;
		}

		bool started = *(bool *)msg.data;

		if (started == true) {
			if (app_connect_get_status() & APP_CONNECT_STATUS_DISCONNECTED) {
				if (app_connect_get_status() & APP_CONNECT_STATUS_AUTH_FAILD) {
					app_player_start(TONE_PLAYER, "/lfs/cloud_auth_fail.mp3");
				} else {
					app_player_start(TONE_PLAYER, "/lfs/064_net_not_connected.mp3");
				}
				continue;
			}

			if (msg.event == APP_CHAT_EVT_WAKEUP_ASR) {
				app_player_lock();
				app_player_start(TONE_PLAYER, "/lfs/000_geeting.mp3");
				app_chat_session_start();
			} else if (msg.event == APP_CHAT_EVT_WAKEUP_BUTTON) {
				app_player_lock();
				// 播放一个空音频
				app_player_start(TONE_PLAYER, "/lfs/end.mp3");
				app_chat_button_wakeup_enable(true);
				app_chat_session_start();
			}

			if (k_timer_remaining_get(&asr_result_timer) != 0) {
				k_timer_stop(&asr_result_timer);
			}
			if (is_in_session == true) {
				// aweui_set_user_asr_result_text(AWEUI_LLM_STATUS_WAITING, NULL, AWEUI_TEXT_MODE_NONE);
			}
		} else {
			if (msg.event == APP_CHAT_EVT_WAKEUP_BUTTON) {
				session_voice_end_audio();
				app_chat_button_wakeup_enable(false);
			}
		}

		if (msg.data != NULL) {
			csk_free(msg.data);
		}
	}
}

static void text_request_cb(int evt, const char *data, void *user)
{
	// static bool is_done = 1;
	LOG_INF("[%s] evt=%d, datas:%s", __FUNCTION__, evt, (char *)data);

	// if (evt == SSE_EVT_DATA) {
	// 	if (strlen((const char *)data) == 0)
	// 		return;

	// 	if (is_done) {
	// 		is_done = 0;
	// 		aweui_set_user_llm_response_result_text(
	// 				AWEUI_LLM_STATUS_SUCCESS, (const char *)data, AWEUI_TEXT_MODE_OVERWRITE);
	// 		#ifdef CONFIG_UART_DISP_EN
	// 		uart_disp_text(data, UART_TEXT_MODE_OVERWRITE);
	// 		#endif
	// 	} else {
	// 		aweui_set_user_llm_response_result_text(
	// 				AWEUI_LLM_STATUS_SUCCESS, (const char *)data, AWEUI_TEXT_MODE_APPEND);
	// 		#ifdef CONFIG_UART_DISP_EN
	// 		uart_disp_text(data, UART_TEXT_MODE_APPEND);
	// 		#endif
	// 	}
	// } else if (evt == SSE_EVT_DONE || evt == SSE_EVT_ABORT) {
	// 	is_done = 1;
	// 	if (strlen((const char *)data) != 0) {
	// 		aweui_set_user_llm_response_result_text(
	// 				AWEUI_LLM_STATUS_SUCCESS, (const char *)data, AWEUI_TEXT_MODE_APPEND);
	// 		#ifdef CONFIG_UART_DISP_EN
	// 		uart_disp_text(data, UART_TEXT_MODE_APPEND);
	// 		#endif
	// 	}
	// }
}

static void nlp_consumer_thread(void *p1, void *p2, void *p3)
{
	app_chat_evt_msg_t msg;
	int ret;
	while (1) {
		ret = k_msgq_get(&nlp_consumer_msgq, &msg, K_FOREVER);
		if (ret != 0) {
			continue;
		}

		if (msg.event == APP_CHAT_EVT_REPLY_TXT) {
			LOG_INF("[%s] get reply text url %s", __FUNCTION__, (char *)msg.data);
			// text_req_ctx = lsc_stream_text_request_new(msg.data, text_request_cb, NULL);
			// if (text_req_ctx) {
			// 	int err = lsc_stream_text_request_start(text_req_ctx, 10);
			// 	if (err) {
			// 		LOG_ERR("lsc_stream_text_request failed, err:%d", err);
			// 	}
			// 	lsc_stream_text_request_delete(text_req_ctx);
			// 	text_req_ctx = NULL;
			// } else {
			// 	LOG_ERR("lsc_stream_text_request_ctx new failed");
			// }
		} else if (msg.event == APP_CHAT_EVT_DRAW) {
			char *url = (char *)msg.data;
			// 适配UI图像框
			char *p = strstr(url, "w_320,h_240");
			if (p != NULL) {
				memmove(p, "w_260,h_195", 11);
			}

			LOG_INF("new LLMDrawing url=%s", url);

			uint8_t *out_data = NULL;
			int out_len = 0;
			int ret = app_download(url, &out_data, &out_len);
			if (ret != 0) {
				LOG_ERR("app download failed: %d", ret);
			} else {
				LOG_INF("app download succeeded: addr: %p, len: %d", (void *)out_data, out_len);
				if ((out_len != 0) && (out_data != NULL)) {
					// aweui_img_t _img;
					// _img.data = out_data;
					// _img.len = out_len;
					// _img.width = 260;
					// _img.height = 195;
					// aweui_set_user_llm_response_result_picture(AWEUI_LLM_STATUS_SUCCESS, (const aweui_img_t)_img);
					app_download_free(out_data);
				}
			}
		}

		if (msg.data != NULL) {
			csk_free(msg.data);
		}
	}
}

static int chat_aiui_session_thread_init(int stack_size, int thread_prio, char *name, k_thread_entry_t entry)
{
	k_thread_stack_t *stack;
	struct k_thread *new_thread;
	k_tid_t tid;

	stack = csk_aligned_alloc(8, stack_size);
	if (stack == NULL) {
		LOG_ERR("[%s] %d malloc failed", __FUNCTION__, __LINE__);
		return -ENOMEM;
	}

	new_thread = csk_malloc(sizeof(struct k_thread));
	if (new_thread == NULL) {
		csk_free(stack);
		return -ENOMEM;
	}

	tid = k_thread_create(new_thread, stack, stack_size, entry, NULL, NULL, NULL, thread_prio, 0, K_NO_WAIT);
	if (tid != new_thread) {
		csk_free(new_thread);
		csk_free(stack);
		LOG_ERR(" thread create failed: %p", tid);
		return -EFAULT;
	}

	k_thread_name_set(tid, name);
	LOG_INF("[%s]thread created successfully", name);

	return 0;
}

int app_chat_init(void)
{
	// app_chat_key_init();

	app_chat_session_init();

	app_music_ctrl_init();

	chat_aiui_session_thread_init(2 * 1024, 2, "consumer_thread", consumer_thread);
	chat_aiui_session_thread_init(3 * 1024, 2, "consumer_iat_thread", consumer_iat_thread);
	chat_aiui_session_thread_init(3 * 1024, 2, "wakeup_inter_thread", wakeup_inter_thread);
	chat_aiui_session_thread_init(4 * 1024, 2, "nlp_reply_text", nlp_consumer_thread);

	k_timer_init(&asr_result_timer, asr_result_timer_timeout_process, NULL);
	// k_work_init(&asr_result_work, asr_result_work_handler);

	// aweui_wakeup_option_t option = WEUI_WAKEUP_OPTION_NONE;
	// get_wakeup_mode_from_ui(&option);
	// if (option == AWEUI_WAKEUP_OPTION_VOICE_SINGLE_TURN) {
		app_chat_session_set_interactive_mode(SESSION_VOICE_INTER_MODE_ONESHOT);
		app_chat_session_vad_enable(true);
	// } else if (option == AWEUI_WAKEUP_OPTION_VOICE_MULTI_TURN) {
	// 	app_chat_session_set_interactive_mode(SESSION_VOICE_INTER_MODE_CONTINUE);
	// 	app_chat_session_vad_enable(true);
	// } else if (option == AWEUI_WAKEUP_OPTION_BUTTON) {
	// 	app_chat_session_set_interactive_mode(SESSION_VOICE_INTER_MODE_ONESHOT);
	// 	app_chat_session_vad_enable(false);
	// }

	app_chat_start();

	return 0;
}

int app_chat_evt_notify(app_chat_evt_e evt, void *data, uint32_t len)
{
	void *msg_data = NULL;
	uint32_t msg_size = len;
	int ret = -1;

	LOG_INF("[%s] evt:%d len:%d", __FUNCTION__, evt, len);

	if (data) {
		msg_data = csk_calloc(1, msg_size);
		if (msg_data == NULL) {
			LOG_ERR("[%s] no mem", __FUNCTION__);
			return -1;
		}
		memcpy(msg_data, data, msg_size);
	}

	app_chat_evt_msg_t msg = {0};
	msg.event = evt;
	msg.data = msg_data;
	msg.len = msg_size;

	switch (evt) {
	case APP_CHAT_EVT_TTS_PLAY:
	case APP_CHAT_EVT_MUSIC_LISTS:
	case APP_CHAT_EVT_MUSIC_INSTR:
	case APP_CHAT_EVT_AIUI_CTRL:
		ret = k_msgq_put(&tts_result_msgq, &msg, K_NO_WAIT);
		if (ret) {
			LOG_INF("[%s] tts_result_msgq put faild(ret=%d)", __FUNCTION__, ret);
		}
		break;
	case APP_CHAT_EVT_IAT:
		ret = k_msgq_put(&iat_result_msgq, &msg, K_NO_WAIT);
		if (ret) {
			LOG_INF("[%s] iat_result_msgq put faild(ret=%d)", __FUNCTION__, ret);
		}
		break;
	case APP_CHAT_EVT_WAKEUP_ASR:
	case APP_CHAT_EVT_WAKEUP_BUTTON:
		ret = k_msgq_put(&wakeup_msgq, &msg, K_NO_WAIT);
		if (ret) {
			LOG_INF("[%s] wakeup_msgq put faild(ret=%d)", __FUNCTION__, ret);
		}
		break;
	case APP_CHAT_EVT_REPLY_TXT:
		ret = k_msgq_put(&nlp_consumer_msgq, &msg, K_NO_WAIT);
		if (ret) {
			LOG_INF("[%s] nlp_consumer_msgq text put faild(ret=%d)", __FUNCTION__, ret);
		}
		break;
	case APP_CHAT_EVT_DRAW:
        lsc_stream_text_request_abort(text_req_ctx);
		ret = k_msgq_put(&nlp_consumer_msgq, &msg, K_NO_WAIT);
		if (ret) {
			LOG_INF("[%s] nlp_consumer_msgq draw put faild(ret=%d)", __FUNCTION__, ret);
		}
		break;
	case APP_CHAT_EVT_FINISH:
	case APP_CHAT_EVT_ERR:
		ret = k_msgq_put(&aiui_result_msgq, &msg, K_NO_WAIT);
		if (ret) {
			LOG_INF("[%s] aiui_result_msgq put faild(ret=%d)", __FUNCTION__, ret);
		}
		break;
    case APP_CHAT_EVT_GOT_VAD:
		lsc_stream_text_request_abort(text_req_ctx);
		break;
	default:
		break;
	}

	return 0;
}

int app_chat_start(void)
{
	LOG_INF("[%s] flag:%d", __FUNCTION__, is_in_session);
	if (is_in_session != true) {
		int ret = app_chat_wakeup_start();
		if (ret) {
			return -1;
		}
		is_in_session = true;
	}
	return 0;
}

int app_chat_stop(void)
{
	LOG_INF("[%s] flag:%d", __FUNCTION__, is_in_session);
	if (is_in_session == true) {
		int ret = app_chat_wakeup_exit();
		if (ret) {
			return -1;
		}
		app_player_stop(TTS_PLAYER);
		app_player_stop(MUSIC_PLAYER);
		app_player_unlock();
		lsc_stream_text_request_abort(text_req_ctx);
		text_req_ctx = NULL;

		// aweui_set_user_asr_result_text(AWEUI_LLM_STATUS_CLOSE, NULL, AWEUI_TEXT_MODE_NONE);
		is_in_session = false;
	}

	return 0;
}

int app_chat_exit(void)
{
	session_voice_end_audio();

	app_player_stop(TTS_PLAYER);
	app_player_stop(MUSIC_PLAYER);
	app_player_unlock();

	k_timer_stop(&asr_result_timer);
	// aweui_set_user_asr_result_text(AWEUI_LLM_STATUS_CLOSE, NULL, AWEUI_TEXT_MODE_NONE);

	lsc_stream_text_request_abort(text_req_ctx);
	text_req_ctx = NULL;
	return 0;
}