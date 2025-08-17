#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/sys/ring_buffer.h>
#include "common.h"
#include "lsc_session_voice.h"
#include "lsc_session_text.h"
#include "lsc_errno.h"
#include "app_chat.h"
#include "lsc_session_request.h"
#include "ico_codec.h"

LOG_MODULE_REGISTER(app_chat_session);

#define RING_BUF_SIZE		(640 * 10)
static struct ring_buf audio_ring_buf;
static uint8_t ring_buffer[RING_BUF_SIZE]__attribute__((section(".psram_section")));

bool if_app_chat_session_is_continue(void)
{
	session_voice_config_t config = {0};

	session_voice_get_config(&config);

	return config.inter_mode == SESSION_VOICE_INTER_MODE_CONTINUE ? true : false;
}

static void voice_event_cb(session_voice_event_e evt, void *data, uint32_t size, void *usr)
{
	LOG_INF("[%s] evt:0x%x", __FUNCTION__, evt);

	app_chat_evt_e notify_evt = APP_CHAT_EVT_NONE;
	switch (evt) {
	case SESSION_VOICE_GOT_VAD:
		notify_evt = APP_CHAT_EVT_GOT_VAD;
		break;
	case SESSION_VOICE_TTS:
		notify_evt = APP_CHAT_EVT_TTS_PLAY;
		break;
	case SESSION_VOICE_IAT:
		notify_evt = APP_CHAT_EVT_IAT;
		break;
	case SESSION_VOICE_REPLY_URL:
		notify_evt = APP_CHAT_EVT_REPLY_TXT;
		break;
	case SESSION_VOICE_MUSIC_LISTS:
		notify_evt = APP_CHAT_EVT_MUSIC_LISTS;
		break;
	case SESSION_VOICE_MUSIC_INSTR:
		notify_evt = APP_CHAT_EVT_MUSIC_INSTR;
		break;
	case SESSION_VOICE_DRAW:
		notify_evt = APP_CHAT_EVT_DRAW;
		break;
	case SESSION_VOICE_FINISH:
		notify_evt = APP_CHAT_EVT_FINISH;
		break;
	case SESSION_VOICE_ERR:
		notify_evt = APP_CHAT_EVT_ERR;
		break;
	case SESSION_VOICE_AIUI_CTRL:
		notify_evt = APP_CHAT_EVT_AIUI_CTRL;
		break;
	default:
		break;
	}

	if (notify_evt != APP_CHAT_EVT_NONE) {
		app_chat_evt_notify(notify_evt, data, size);
	}
}

int app_chat_session_add_cb(void)
{
	return session_voice_add_evt_callback(voice_event_cb,
			SESSION_VOICE_GOT_VAD | SESSION_VOICE_TTS | SESSION_VOICE_IAT | SESSION_VOICE_REPLY_URL |
					SESSION_VOICE_AIUI_CTRL | SESSION_VOICE_DRAW | SESSION_VOICE_MUSIC_LISTS |
					SESSION_VOICE_MUSIC_INSTR | SESSION_VOICE_FINISH,
			NULL);
}

int app_chat_session_delete_cb(void)
{
	return session_voice_remove_evt_callback(voice_event_cb);
}

int app_chat_session_init(void)
{
	int ret = 0;

	ret = session_voice_init();
	ret = session_text_init();
	ret = session_request_init();

	ret = session_voice_add_evt_callback(voice_event_cb,
			SESSION_VOICE_GOT_VAD | SESSION_VOICE_TTS | SESSION_VOICE_IAT | SESSION_VOICE_REPLY_URL |
					SESSION_VOICE_AIUI_CTRL | SESSION_VOICE_DRAW | SESSION_VOICE_MUSIC_LISTS |
					SESSION_VOICE_MUSIC_INSTR | SESSION_VOICE_FINISH,
			NULL);

	char m_device_id[24] = {0};

	extern bool get_device_identity(char *id, int id_max_len);
	get_device_identity(m_device_id, sizeof(m_device_id));

	// LOG_INF("[%s]device_identity: %s", __FUNCTION__, m_device_id);

	session_voice_config_t config = {
		.vad_enable = true,
		.speex_size = 0,
		.aue = "ico",
		.inter_mode = SESSION_VOICE_INTER_MODE_ONESHOT,
		.session_timeout_ms = 20000,
	};
	strncpy(config.device_id, m_device_id, sizeof(config.device_id));

	ret = session_voice_set_config(&config);

	ret = ico_encode_init();

	ring_buf_init(&audio_ring_buf, RING_BUF_SIZE, ring_buffer);

	return 0;
}

int app_chat_session_vad_enable(bool en)
{
	int ret = 0;

	session_voice_config_t config = {0};
	ret = session_voice_get_config(&config);
	if (ret != 0) {
		LOG_INF("[%s] session_voice_get_config failed", __FUNCTION__);
		return -1;
	}

	config.vad_enable = en;
	ret = session_voice_set_config(&config);
	if (ret != 0) {
		LOG_INF("[%s] session_voice_set_config failed", __FUNCTION__);
		return -1;
	}

	return 0;
}

#define AUDIO_STREAM_PCM_FRAME_SIZE (640)
#define AUDIO_STREAM_ICO_FRAME_SIZE (40)
uint8_t pcm_data[AUDIO_STREAM_PCM_FRAME_SIZE] = {0};

void app_chat_session_send_audio(uint8_t *data, uint32_t len)
{
	uint8_t ico_data[AUDIO_STREAM_ICO_FRAME_SIZE] = {0};
	short ico_enc_len = 0;
	int ret = 0;

	if (ring_buf_space_get(&audio_ring_buf) < len) {
		LOG_ERR("audio ring buffer overflow!!!!");
	}
	ring_buf_put(&audio_ring_buf, data, len);

	while (ring_buf_size_get(&audio_ring_buf) >= AUDIO_STREAM_PCM_FRAME_SIZE) {
		ring_buf_get(&audio_ring_buf, pcm_data, AUDIO_STREAM_PCM_FRAME_SIZE);
		
		ret = ico_codec_encode((short *)pcm_data, (void *)&ico_data[0], &ico_enc_len);
		if(ret != ivErr_OK) {
			LOG_WRN("[%s] ico codec encode fail ret: %d", __FUNCTION__, ret);
		}

		ico_enc_len <<= 1;
		session_voice_send_audio(ico_data, ico_enc_len);
	}

}

int app_chat_session_clear_memory(void)
{
	return session_text_send("清除记忆");
}

int app_chat_revert_precision_recognition_mode(void)
{
	return session_text_send("恢复到精准识别模式");
}

int app_chat_session_clear_history(void)
{
	return session_text_send("清除历史记录");
}

int app_chat_session_set_interactive_mode(session_voice_interactive_mode_e mode)
{
	int ret = 0;

	session_voice_config_t config = {0};
	ret = session_voice_get_config(&config);
	if (ret != 0) {
		LOG_INF("[%s] session_voice_get_config failed", __FUNCTION__);
		return -1;
	}

	config.inter_mode = mode;
	ret = session_voice_set_config(&config);
	if (ret != 0) {
		LOG_INF("[%s] session_voice_set_config failed", __FUNCTION__);
		return -1;
	}

	return 0;
}

void app_chat_session_start(void)
{
	int ret;
	ret = session_voice_start();
	if(ret != LSC_OK){
		return;
	}
	ring_buf_reset(&audio_ring_buf);
}