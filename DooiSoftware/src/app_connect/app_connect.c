#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/hwinfo.h>
#include <ctype.h>
#include "app_connect.h"
#include "lsc.h"
#include "wifi_mgr.h"
#include "ls_cmd.h"
#include "lsc_conn.h"
#include "hw_info.h"
#include "stdio.h"
#include "csk_malloc.h"
#include <zephyr/posix/unistd.h>

LOG_MODULE_REGISTER(app_connect);

static K_SEM_DEFINE(connected_sem, 0, 1);
static K_SEM_DEFINE(reconnected_sem, 0, 1);
static K_SEM_DEFINE(disconnected_sem, 0, 1);
static K_SEM_DEFINE(auth_err_sem, 0, 1);
static app_connect_status_e connect_status =  APP_CONNECT_STATUS_DISCONNECTED;
static int app_connect(void);

bool get_device_identity(char *id, int id_max_len)
{
	uint8_t hwinfo_id[8];
	ssize_t length;

	length = hwinfo_get_device_id(hwinfo_id, sizeof(hwinfo_id));
	if (length <= 0) {
		return false;
	}

	memset(id, 0, id_max_len);
	length = bin2hex(hwinfo_id, (size_t)length, id, id_max_len);

	// convert to upper case
	for (size_t i = 0; i < length; i++) {
		id[i] = toupper(id[i]);
	}

	return length > 0;
}

static void lsc_event_cb(lsc_event_e evt, void *data, uint32_t size, void *usr)
{
	LOG_INF("lsc event:%d", evt);
	switch (evt) {
	case LSC_CONNECTED:
		k_sem_give(&connected_sem);
        connect_status = APP_CONNECT_STATUS_CONNECTED;
		break;
    case LSC_DISCONNECTED:
        k_sem_give(&disconnected_sem);
        connect_status = APP_CONNECT_STATUS_DISCONNECTED;
        break;
    case LSC_CLOUD_AUTH_FAILD:
        k_sem_give(&auth_err_sem);
        connect_status |= APP_CONNECT_STATUS_AUTH_FAILD;
        break;
    case LSC_GOT_TOKEN:
        connect_status &= ~APP_CONNECT_STATUS_AUTH_FAILD;
        break;
	default:
		break;
	}
}

int device_info_read(device_info_t *inf)
{
	chip_id_read(inf->chip_id, sizeof(inf->chip_id));
	if (0 > ls_cmd_nvs_get_value(PRODUCT_ID, inf->product_id, sizeof(inf->product_id))) {
		snprintf(inf->product_id, sizeof(inf->product_id), "%s", CONFIG_PRODUCT_ID_STRING);
		LOG_WRN("Read product id failed,use default:%s", CONFIG_PRODUCT_ID_STRING);
	}

	if (0 > ls_cmd_nvs_get_value(SECRET_ID, inf->secret_id, sizeof(inf->secret_id))) {
		snprintf(inf->secret_id, sizeof(inf->secret_id), "%s", CONFIG_SECRET_ID_STRING);
		LOG_WRN("Read secret id failed,use default:%s", CONFIG_SECRET_ID_STRING);
	}

	return 0;
}

int app_connect_init(void)
{
	/* network connection */
	wifi_init(NULL, NULL);
	int ret = app_connect();
	return ret;
}


app_connect_status_e app_connect_get_status(void)
{
	return connect_status;
}
static int app_connect(void)
{
	bool host_status = false;
	char m_device_id[24] = {0};
	get_device_identity(m_device_id, sizeof(m_device_id));

	LOG_INF("device_identity: %s", m_device_id);

	lsc_config_t cfg = {
			.device_id = m_device_id,
			.product_id = CONFIG_PRODUCT_ID_STRING,
			.secret_id = CONFIG_SECRET_ID_STRING,
			.if_auto_reconn = true,
			.reconn_interval_ms = 1000,
	};
	device_info_t info;
	device_info_read(&info);

	cfg.device_id = info.chip_id;
	cfg.product_id = info.product_id;
	cfg.secret_id = info.secret_id;
	int err = lsc_init(&cfg);
	if (err) {
		LOG_ERR("lsc_reinit failed, err:%d", err);
		return -1;
	}
	ls_cmd_nvs_get_value(HOST_STA, &host_status, sizeof(bool));
	lsc_set_host_status(host_status);

	err = lsc_add_callback(LSC_CONNECTED | LSC_DISCONNECTED | LSC_CLOUD_AUTH_FAILD | LSC_GOT_TOKEN, lsc_event_cb, NULL);
	if (err) {
		LOG_ERR("lsc_add_callback failed, err:%d", err);
		return -1;
	}

	err = lsc_connect();
	if (err) {
		LOG_ERR("lsc_connect failed, err:%d", err);
		return -1;
	}
	return 0;
}
int app_reconnect(void)
{
	k_sem_give(&reconnected_sem);
	return 0;
}
void app_reconnected(void)
{
	int ret = lsc_disconnect();
	if (ret) {
		LOG_ERR("lsc_disconnect failed, err:%d", ret);
	}
	k_sem_take(&disconnected_sem, K_MSEC(3000));

	lsc_config_t cfg = {
			.product_id = CONFIG_PRODUCT_ID_STRING,
			.secret_id = CONFIG_SECRET_ID_STRING,
			.if_auto_reconn = true,
			.reconn_interval_ms = 1000,
	};
	device_info_t info;
	device_info_read(&info);

	cfg.device_id = info.chip_id;
	cfg.product_id = info.product_id;
	cfg.secret_id = info.secret_id;

	lsc_set_config(&cfg);
}

void re_connect_task(void *p1, void *p2, void *p3)
{
	while (1) {
		k_sem_take(&reconnected_sem, K_FOREVER);
		LOG_INF("Re connect");
		app_reconnected();
	}
}

static int re_connect_task_init(void)
{
    k_thread_stack_t *stack;
    k_tid_t tid;
    const int stack_size = 3 * 1024;
    const int thread_prio = 6;

    stack = csk_aligned_alloc(8, stack_size);
    if (stack == NULL) {
        LOG_ERR("re_connect_taskr stack malloc failed");
        return -ENOMEM;
    }

    struct k_thread *new_thread = csk_malloc(sizeof(struct k_thread));
    if (new_thread == NULL) {
        csk_free(stack);
        return -ENOMEM;
    }

    tid = k_thread_create(new_thread, stack, stack_size, re_connect_task, 
                         NULL, NULL, NULL, thread_prio, 0, K_NO_WAIT);
    if (tid != new_thread) {
        csk_free(new_thread);
        csk_free(stack);
        LOG_ERR("re_connect_task thread creation failed");
        return -EFAULT;
    }

    k_thread_name_set(tid, "re_connect_task");
    return 0;
}

SYS_INIT(re_connect_task_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);


