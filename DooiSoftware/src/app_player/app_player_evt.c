#include <zephyr/kernel.h>
#include <stdio.h>
#include <stdlib.h>
#include "interface.h"
#include "csk_malloc.h"
#include "lisa_mem.h"
#include "app_player.h"
#include "app_player_evt.h"
#include "pa_mute.h"
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(app_player, LOG_LEVEL_INF);

K_MSGQ_DEFINE(s_app_player_evt_msg, sizeof(app_player_evt_msg), 10, 4);

extern void dispatch_player_status_cb(int id, ap_status_event_e event);
extern void pause_work_handler(int id);
extern void error_work_handler(int id);
extern void _complete_work_handler(int id);

int app_player_evt_notify(int id, PlayerEvt evt)
{
	app_player_evt_msg msg;
	msg.id = id;
	msg.evt = evt;

	int ret = k_msgq_put(&s_app_player_evt_msg, &msg, K_NO_WAIT);
	if(ret < 0){
		LOG_ERR("[%s] msg put err(ret=%d)", __FUNCTION__, ret);
		return -1;
	}

	return 0;
}

static void player_evt_proc_thread(void *p1, void *p2, void *p3)
{
	app_player_evt_msg msg;

	while(1){
		if(0 < k_msgq_get(&s_app_player_evt_msg, &msg, K_FOREVER)) {
            continue;
        }

		int id = msg.id;
		LOG_INF("[%s] id:%d evt:%d", __FUNCTION__, id, msg.evt);

		switch(msg.evt){
			case PLAYER_EVT_PREPARED:
				dispatch_player_status_cb(id, msg.evt);
			case PLAYER_EVT_PLAYING:
				dispatch_player_status_cb(id, msg.evt);
				break;
			case PLAYER_EVT_PAUSED:
				dispatch_player_status_cb(id, msg.evt);
				pause_work_handler(id);
				break;
			case PLAYER_EVT_STOPED:
				dispatch_player_status_cb(id, msg.evt);
				break;
			case PLAYER_EVT_PLAYBACK_COMPLETE:
				dispatch_player_status_cb(id, msg.evt);
				_complete_work_handler(id);
				break;
			case PLAYER_EVT_ERROR://Fixme: need to refactor
				error_work_handler(id);
				dispatch_player_status_cb(id, msg.evt);
				break;
			default:
				LOG_INF("-----------------------  evt:%d  id:%d", msg.evt, id);
				break;
		}
	}
}

app_player_err_e player_evt_proc_init(void)
{
	k_thread_stack_t *stack;
	k_tid_t tid;
	const int stack_size = 2 * 1024;
	const int thread_prio = 9;

	stack = csk_aligned_alloc(8,stack_size);
	if (stack == NULL) {
		LOG_ERR("stack malloc failed");
		return APP_PLAYER_NO_MEMORY;
	}

	struct k_thread *new_thread = csk_malloc(sizeof(struct k_thread));
	if (new_thread == NULL) {
		csk_free(stack);
		return APP_PLAYER_NO_MEMORY;
	}

	tid = k_thread_create(new_thread, stack, stack_size, player_evt_proc_thread, NULL, NULL, NULL,
			      thread_prio, 0, K_NO_WAIT);
	if (tid != new_thread) {
		csk_free(new_thread);
		csk_free(stack);
		LOG_ERR("player_evt_proc thread create failed: %p", tid);
		return APP_PLAYER_ERR;
	}

	k_thread_name_set(tid, "player_evt_proc");
	LOG_INF("player_evt_proc thread created successfully");

	return APP_PLAYER_OK;
}
