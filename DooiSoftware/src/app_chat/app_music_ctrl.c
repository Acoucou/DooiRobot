#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "player_manager.h"
#include "lsc_session_voice.h"
#include "lsc_errno.h"
#include "app_chat.h"
#include "lsc_stream_text.h"
#include "csk_malloc.h"
#include "app_connect.h"
#include "app_chat_session.h"
#include "app_chat_wakeup.h"
#include "app_download.h"
#include "lsc_session_request.h"
#include <stdlib.h>
#include "cJSON.h"
#include "screen.h"
#include "app_peripherals.h"
#include "app_obj_rec.h"
#include "ble_connect.h"
#include "action_sequences.h"
LOG_MODULE_REGISTER(app_player_ctrl);

#define MUSIC_VOLUME_STEP (10)

typedef enum {
	MUSIC_VOLUME_MIN = 0,
	MUSIC_VOLUME_MID = 50,
	MUSIC_VOLUME_DEFAULT = 80,
	MUSIC_VOLUME_MAX = 100,
} music_volume_e;

typedef struct {
	char id[64];
	char url[256];
} music_item_t;

static music_item_t *s_music_list = NULL;
int s_music_item_cnt = 0;
int s_playing_index = 0;
static struct k_work music_url_play_work;

static void request_music_url_and_play_bak(int index)
{
	if (s_music_list == NULL) {
		LOG_WRN("No music list");
		return;
	}

	if (strlen(s_music_list[index].url) == 0) {
		int ret = lsc_music_request_url(s_music_list[index].id, s_music_list[index].url);
		if (ret < 0) {
			LOG_ERR("request_music_url failed: %d", ret);
			return;
		} else {
			LOG_INF("get music url: %s", s_music_list[index].url);
		}
	} else {
		LOG_INF("already music url: %s", s_music_list[index].url);
	}

	app_player_start(MUSIC_PLAYER, s_music_list[index].url);

	return;
}

static void music_url_play_handler(struct k_work *work)
{
	request_music_url_and_play_bak(s_playing_index);
}

void request_music_url_and_play(int index)
{
	if (s_music_list == NULL) {
		LOG_WRN("No music list");
		return;
	}
	k_work_submit(&music_url_play_work);
}

static void app_music_player_cb(int player_id, app_player_event_e evt, void *arg)
{
	LOG_INF("[play_id:%d] evt:%d", player_id, evt);
	switch (evt) {
	case APP_PLAYER_EVT_PREPARED:
		break;
	case APP_PLAYER_EVT_PLAYING:
		// aweui_set_audio_player_status(AWEUI_AUDIO_PLAYER_STATUS_PLAYING);
		break;
	case APP_PLAYER_EVT_PAUSED:
		// aweui_set_audio_player_status(AWEUI_AUDIO_PLAYER_STATUS_PAUSED);
		break;
	case APP_PLAYER_EVT_STOPED:
		// aweui_set_audio_player_status(AWEUI_AUDIO_PLAYER_STATUS_STOPPED);
		break;
	case APP_PLAYER_EVT_ERROR:
	case APP_PLAYER_EVT_PLAYBACK_COMPLETE:
		if ((s_playing_index + 1) < s_music_item_cnt) {
			s_playing_index++;
			LOG_INF("playing_index: %d", s_playing_index);
			request_music_url_and_play(s_playing_index);
		}
		// aweui_set_audio_player_status(AWEUI_AUDIO_PLAYER_STATUS_STOPPED);
		break;

	default:
		break;
	}
}
void handle_scenario(const char *scenario) {
    // 基础表情处理
    if (strcmp(scenario, "surprised") == 0) {
        action_sequences_enqueue(SEQUENCE_SURPRISED);
    } 
    else if (strcmp(scenario, "angry") == 0) {
        action_sequences_enqueue(SEQUENCE_ANGRY);
    } 
    else if (strcmp(scenario, "happyJump") == 0) {
        action_sequences_enqueue(SEQUENCE_HAPPY);
    } 
    else if (strcmp(scenario, "sad") == 0) {
        action_sequences_enqueue(SEQUENCE_SAD);
    } 
    else if (strcmp(scenario, "cute") == 0) {
        action_sequences_enqueue(SEQUENCE_CUTE);
    } 
    
    // 肢体动作处理
    else if (strcmp(scenario, "danceRoutine") == 0) {
        action_sequences_enqueue(SEQUENCE_DANCE_ROUTINE);
    } 
    else if (strcmp(scenario, "greeting") == 0) {
        action_sequences_enqueue(SEQUENCE_GREETING);
    } 
    else if (strcmp(scenario, "waveGoodbye") == 0) {
        action_sequences_enqueue(SEQUENCE_WAVE_GOODBYE);
    } 
    else if (strcmp(scenario, "hug") == 0) {
        action_sequences_enqueue(SEQUENCE_HUG);
    } 
    else if (strcmp(scenario, "nodAgree") == 0) {
        action_sequences_enqueue(SEQUENCE_NOD_AGREE);
    } 
    else if (strcmp(scenario, "shakeNo") == 0) {
        action_sequences_enqueue(SEQUENCE_SHAKE_NO);
    } 
    else if (strcmp(scenario, "victory") == 0) {
        action_sequences_enqueue(SEQUENCE_VICTORY);
    } 
    
    // 认知状态处理
    else if (strcmp(scenario, "thinking") == 0) {
        action_sequences_enqueue(SEQUENCE_THINKING);
    } 
    else if (strcmp(scenario, "confused") == 0) {
        action_sequences_enqueue(SEQUENCE_CONFUSED);
    } 
    else if (strcmp(scenario, "leftEyeFocus") == 0) {
        action_sequences_enqueue(SEQUENCE_LEFT_EYE_FOCUS);
    } 
    else if (strcmp(scenario, "rightEyeFocus") == 0) {
        action_sequences_enqueue(SEQUENCE_RIGHT_EYE_FOCUS);
    } 
    else if (strcmp(scenario, "patrol") == 0) {
        action_sequences_enqueue(SEQUENCE_PATROL);
    } 
    else if (strcmp(scenario, "search") == 0) {
        action_sequences_enqueue(SEQUENCE_SEARCH);
    } 
    
    // 生理状态处理
    else if (strcmp(scenario, "sleep") == 0) {
        action_sequences_enqueue(SEQUENCE_SLEEP);
    } 
    else if (strcmp(scenario, "charging") == 0) {
        action_sequences_enqueue(SEQUENCE_CHARGING);
    } 
    else if (strcmp(scenario, "lowBattery") == 0) {
        action_sequences_enqueue(SEQUENCE_LOW_BATTERY);
    } 
    
    // 特殊互动处理
    else if (strcmp(scenario, "affectation") == 0) {
        action_sequences_enqueue(SEQUENCE_COMBO_ACTION);
    } 
    else if (strcmp(scenario, "cry") == 0) {
        action_sequences_enqueue(SEQUENCE_CRY);
    } 
    else if (strcmp(scenario, "disdain") == 0) {
        action_sequences_enqueue(SEQUENCE_DISDAIN);
    } 
    else if (strcmp(scenario, "fear") == 0) {
        action_sequences_enqueue(SEQUENCE_FEAR_EXTENDED);
    } 
    else if (strcmp(scenario, "shy") == 0) {
        action_sequences_enqueue(SEQUENCE_SHY);
    } 
    else {
        LOG_WRN("Unknown scenario: %s", scenario);
    }
}

void handle_control(const char *control) {
    if (strcmp(control, "forward") == 0) {
        motor_motion_control(0, -1, 1000);
    } 
    else if (strcmp(control, "backward") == 0) {
        motor_motion_control(0, 1, 1000);
    } 
    else if (strcmp(control, "turnLeft") == 0) {
        motor_motion_control(-1, 0, 1000);
    }
    else if (strcmp(control, "turnRight") == 0) {
        motor_motion_control(1, 0, 1000);
    }
    else if (strcmp(control, "openCamera") == 0) {
        camera_func_set(CAMERA_SHOW);
        screen_set_current(SCREEN_CAMERA);
    }
    else if (strcmp(control, "closeCamera") == 0) {
        screen_set_current(SCREEN_EYES);
    }
	else if (strcmp(control, "takePicture") == 0) {
        camera_func_set(CAMERA_TAKE_PIC);
        screen_set_current(SCREEN_CAMERA);
    }
    else if (strcmp(control, "objectRecognition") == 0) {
        camera_func_set(CAMERA_OBJ_REC);
        screen_set_current(SCREEN_CAMERA);
    }
    else if (strcmp(control, "QRCodeRecognition") == 0) {
        camera_func_set(CAMERA_QRCODE);
        screen_set_current(SCREEN_CAMERA);
    }
    else if (strcmp(control, "musicSpectrum") == 0) {
        screen_set_current(SCREEN_MUSIC_SPECTRUM);
    }
	else if (strcmp(control, "musicStop") == 0) {
        app_player_stop(MUSIC_PLAYER);
        screen_set_current(SCREEN_EYES);
    }
    else if (strcmp(control, "volumeDown") == 0) {
        int volume = 0;
		app_player_get_volume(MUSIC_PLAYER, &volume);
		volume -= MUSIC_VOLUME_STEP;
		if (volume < MUSIC_VOLUME_MIN)
			volume = MUSIC_VOLUME_MIN;
		app_player_set_sys_volume(volume);
    }
	else if (strcmp(control, "volumeUp") == 0) {
        int volume = 0;
		app_player_get_volume(MUSIC_PLAYER, &volume);
		volume += MUSIC_VOLUME_STEP;
		if (volume > MUSIC_VOLUME_MAX)
			volume = MUSIC_VOLUME_MAX;
		app_player_set_sys_volume(volume);
    }
    else if (strcmp(control, "tomatoClock") == 0) {
        digital_clock_mode_set(MODE_TIMER);
        screen_set_current(SCREEN_DIGITAL_CLOCK);
    }
    else if (strcmp(control, "currentClock") == 0) {
        digital_clock_mode_set(MODE_TIME);
        screen_set_current(SCREEN_DIGITAL_CLOCK);
    }
    else if (strcmp(control, "luckyCat") == 0) {
        screen_set_current(SCREEN_LUCKY_CAT);
    }
	else if (strcmp(control, "bluetoothOpen") == 0) {
		ble_start_adv();
    }
	else if (strcmp(control, "bluetoothClose") == 0) {
		ble_stop_adv();
    }
	else if (strcmp(control, "conversation_open") == 0) {
		app_chat_session_set_interactive_mode(SESSION_VOICE_INTER_MODE_CONTINUE);
	}
	else if (strcmp(control, "conversation_close") == 0) {
		app_chat_session_set_interactive_mode(SESSION_VOICE_INTER_MODE_ONESHOT);
	}
    else {
        LOG_INF("Unknown control: %s", control);
    }
}

// 重构JSON解析函数
void handle_session_voice_aiui_ctrl(const char *json_string) {
    cJSON *json = cJSON_Parse(json_string);
    if (!json) {
        LOG_INF("Failed to parse JSON");
        return;
    }

    // 解析控制指令
    cJSON *control_item = cJSON_GetObjectItem(json, "control");
    if (cJSON_IsString(control_item) && control_item->valuestring[0] != '\0') {
        handle_control(control_item->valuestring);
    }

    // 解析情境响应
    cJSON *scenario_item = cJSON_GetObjectItem(json, "scenario");
    if (cJSON_IsString(scenario_item) && scenario_item->valuestring[0] != '\0') {
        handle_scenario(scenario_item->valuestring);
    }

    cJSON_Delete(json);
}


extern struct k_msgq tts_result_msgq;
static void app_player_ctrl_thread(void *p1, void *p2, void *p3)
{
	int ret = 0;

	app_chat_evt_msg_t msg;

	app_player_set_sys_volume(60); 	// 设置默认音量
	
	while (1) {
		ret = k_msgq_get(&tts_result_msgq, &msg, K_FOREVER);
		if (ret != 0) {
			continue;
		}

		LOG_INF("[%s] get msg evt: %d", __FUNCTION__, msg.event);

		if (msg.event == APP_CHAT_EVT_TTS_PLAY) {
			app_player_start(TTS_PLAYER, msg.data);
			app_player_unlock();
		} else if (msg.event == APP_CHAT_EVT_MUSIC_LISTS) {
			session_voice_music_lists_t *music_lists = (session_voice_music_lists_t *)msg.data;
			music_item_t *music_items = csk_calloc(music_lists->cnt, sizeof(music_item_t));
			for (int i = 0; i < music_lists->cnt; i++) {
				strcpy(music_items[i].id, music_lists->items[i].id);
				LOG_INF("music_%d (id:%s)", i, music_lists->items[i].id);
			}

			if (s_music_list)
				csk_free(s_music_list);
			s_music_list = music_items;
			s_music_item_cnt = music_lists->cnt;

			s_playing_index = 0;
			request_music_url_and_play(s_playing_index);

			screen_set_current(SCREEN_MUSIC_SPECTRUM);
		} else if (msg.event == APP_CHAT_EVT_MUSIC_INSTR) {
			session_voice_music_instr_t *music_instr = (session_voice_music_instr_t *)msg.data;
			if (music_instr->instr == SESSION_VOICE_MUSIC_INSTR_NEXT) {
				if ((s_playing_index + 1) < s_music_item_cnt) {
					s_playing_index++;
				}
				request_music_url_and_play(s_playing_index);
			} else if (music_instr->instr == SESSION_VOICE_MUSIC_INSTR_PAST) {
				if ((s_playing_index - 1) >= 0) {
					s_playing_index--;
				}
				request_music_url_and_play(s_playing_index);
			} else if (music_instr->instr == SESSION_VOICE_MUSIC_INSTR_REPLAY) {
				request_music_url_and_play(s_playing_index);
			} else if (music_instr->instr == SESSION_VOICE_MUSIC_INSTR_CLOSE) {
				app_player_stop(MUSIC_PLAYER);
			} else if (music_instr->instr == SESSION_VOICE_MUSIC_INSTR_VOL_SET) {
				app_player_set_sys_volume(music_instr->arg);
			}
		} else if (msg.event == APP_CHAT_EVT_AIUI_CTRL) {

			char *aiui_strings = (char *)msg.data;
			LOG_INF("APP_CHAT_EVT_AIUI_CTRL : %s", aiui_strings);
			handle_session_voice_aiui_ctrl((const char *)aiui_strings);

// 			char *aiui_strings = (char *)msg.data;
// 			char command[100];
// 			char parameter[100];

// 			// 使用strtok分割字符串
// 			char *token = strtok(aiui_strings, ":");
// 			if (token != NULL) {
// 				strcpy(command, token); // 获取命令部分
// 				token = strtok(NULL, ":");
// 				if (token != NULL) {
// 					strcpy(parameter, token); // 获取参数部分
// 				}
// 			}
// 			char *aiui_ctrl_strings = (char *)command;
// 			if (!strcmp(aiui_ctrl_strings, "PAUSE")) { // 暂停
// 				app_player_pause(MUSIC_PLAYER);
// 			} else if (!strcmp(aiui_ctrl_strings, "RESUME_PLAY")) { // 继续
// 				app_player_resume(MUSIC_PLAYER);
// 			} else if (!strcmp(aiui_ctrl_strings, "CHOOSE_NEXT")) { // 下一首
// 				if ((s_playing_index + 1) < s_music_item_cnt) {
// 					s_playing_index++;
// 				}
// 				LOG_INF("%s, %d", aiui_ctrl_strings, __LINE__);
// 				request_music_url_and_play(s_playing_index);

// 			} else if (!strcmp(aiui_ctrl_strings, "CHOOSE_PREVIOUS")) { // 上一首
// 				if ((s_playing_index - 1) >= 0) {
// 					s_playing_index--;
// 				}

// 				LOG_INF("%s, %d", aiui_ctrl_strings, __LINE__);
// 				request_music_url_and_play(s_playing_index);

// 			} else if (!strcmp(aiui_ctrl_strings, "REPLAY")) { // 重播

// 				LOG_INF("%s, %d", aiui_ctrl_strings, __LINE__);
// 				request_music_url_and_play(s_playing_index);

// 			} else if (!strcmp(aiui_ctrl_strings, "close")) { // 停止音乐
// 				app_player_stop(MUSIC_PLAYER);
// 			} else if (!strcmp(aiui_ctrl_strings, "EXIT") || !strcmp(aiui_ctrl_strings, "MUTE")) { // 退出交互
// 				app_chat_exit();
// 			} else if (!strcmp(aiui_ctrl_strings, "VOLUME_MINUS")) { // 声音小
// 				int volume = 0;
// 				app_player_get_volume(MUSIC_PLAYER, &volume);
// 				volume -= MUSIC_VOLUME_STEP;
// 				if (volume < MUSIC_VOLUME_MIN)
// 					volume = MUSIC_VOLUME_MIN;
// 				app_player_set_sys_volume(volume);
// 			} else if (!strcmp(aiui_ctrl_strings, "VOLUME_PLUS")) { // 声音大
// 				int volume = 0;
// 				app_player_get_volume(MUSIC_PLAYER, &volume);
// 				volume += MUSIC_VOLUME_STEP;
// 				if (volume > MUSIC_VOLUME_MAX)
// 					volume = MUSIC_VOLUME_MAX;
// 				app_player_set_sys_volume(volume);
// 			} else if (!strcmp(aiui_ctrl_strings, "UNMUTE")) { // 取消静音
// 				int volume = 0;
// 				app_player_get_volume(MUSIC_PLAYER, &volume);
// 				if (volume == 0) {
// 					app_player_set_sys_volume(MUSIC_VOLUME_DEFAULT);
// 				}

// 			} else if (!strcmp(aiui_ctrl_strings, "VOLUME_MAX")) { // 音量最大
// 				app_player_set_sys_volume(MUSIC_VOLUME_MAX);
// 			} else if (!strcmp(aiui_ctrl_strings, "VOLUME_MIN")) { // 音量最小
// 				app_player_set_sys_volume(MUSIC_VOLUME_MIN);
// 			} else if (!strcmp(aiui_ctrl_strings, "VOLUME_MID")) { // 音量中等
// 				app_player_set_sys_volume(MUSIC_VOLUME_MID);
// 			} else if (!strcmp(aiui_ctrl_strings, "VOLUME_QUERY")) { // 音量查询
// #define VOL_TTS_FORMAT "[se50][ve80]当前音量为百分之%d"

// 				char *buffer = csk_malloc(strlen(VOL_TTS_FORMAT) + 10);
// 				if (buffer == NULL) {
// 					LOG_ERR("%s %d malloc failed", __FUNCTION__, __LINE__);
// 					return;
// 				}

// 				int volume = 0;
// 				app_player_get_volume(MUSIC_PLAYER, &volume);
// 				int size = sprintf(buffer, VOL_TTS_FORMAT, volume);
// 				buffer[size] = '\0';

// 				LOG_INF("VOLUME_QUERY: %s size: %d", buffer, size);

// 				char tts_url[256];
// 				int ret = session_request_xtts(buffer, tts_url, 2000);
// 				if (ret == 0) {
// 					app_chat_evt_notify(APP_CHAT_EVT_TTS_PLAY, tts_url, strlen(tts_url) + 1);
// 				}

// 				csk_free(buffer);

// 			} else if (!strcmp(aiui_ctrl_strings, "VOLUME_SET")) { // 设置音量值
// 				int volume = atoi(parameter);
// 				volume = volume > MUSIC_VOLUME_MAX   ? MUSIC_VOLUME_MAX
// 						 : volume < MUSIC_VOLUME_MIN ? MUSIC_VOLUME_MIN
// 													 : volume;
// 				app_player_set_sys_volume(volume);

// 			} else {
// 				LOG_ERR("unkown AIUI.control cmd: %s", aiui_ctrl_strings);
// 			}
		}

		if (msg.data != NULL) {
			csk_free(msg.data);
		}
	}
}

int app_music_ctrl_init(void)
{
	k_thread_stack_t *stack;
	struct k_thread *new_thread;
	k_tid_t tid;

	int stack_size = 4 * 1024;
	int thread_prio = 2;

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

	tid = k_thread_create(
			new_thread, stack, stack_size, app_player_ctrl_thread, NULL, NULL, NULL, thread_prio, 0, K_NO_WAIT);
	if (tid != new_thread) {
		csk_free(new_thread);
		csk_free(stack);
		LOG_ERR(" thread create failed: %p", tid);
		return -EFAULT;
	}

	k_thread_name_set(tid, "app_player_ctrl_thread");
	LOG_INF("[%s]thread created successfully", "app_player_ctrl_thread");

	lsc_music_active();
	k_work_init(&music_url_play_work, music_url_play_handler);
	app_player_register_callback(MUSIC_PLAYER, app_music_player_cb, NULL);

	return 0;
}
