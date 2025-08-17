#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>
#include <stdio.h>
#include "player_manager.h"
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/settings/settings.h>
#include "cJSON.h"
#include "ls_cmd.h"
#include "hw_info.h"
#include "csk_malloc.h"

LOG_MODULE_REGISTER(ble_cnn, LOG_LEVEL_INF);
typedef struct {
	char ssid[64];
	char password[64];
	char auth[64];
} wifi_kb_scan_result;
struct wifi_kb_connection_result {
	wifi_kb_scan_result *scan_result;
	const char *err_msg;
	int32_t err_code;
};


K_SEM_DEFINE(ble_sem, 0, 1);

// 自定义128位UUID
#define WIFI_SERVICE_UUID_VAL BT_UUID_128_ENCODE(0x0000fae0, 0x0, 0x1000, 0x8000, 0x00805f9b34fb)
#define WIFI_WRITE_UUID_VAL BT_UUID_128_ENCODE(0x0000fae1, 0x0, 0x1000, 0x8000, 0x00805f9b34fb)
#define WIFI_NOTIFY_UUID_VAL BT_UUID_128_ENCODE(0x0000fae2, 0x0, 0x1000, 0x8000, 0x00805f9b34fb)

static struct bt_uuid_128 wifi_service_uuid = BT_UUID_INIT_128(WIFI_SERVICE_UUID_VAL);
static struct bt_uuid_128 wifi_write_uuid = BT_UUID_INIT_128(WIFI_WRITE_UUID_VAL);
static struct bt_uuid_128 wifi_notify_uuid = BT_UUID_INIT_128(WIFI_NOTIFY_UUID_VAL);

static void wifi_notify_changed(struct bt_gatt_attr *attr, uint16_t value);
static ssize_t read_signed(
		struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset);
static ssize_t write_wifi_data(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len,
		uint16_t offset, uint8_t flags);
int custom_ble_config_datas_response(uint8_t code, uint8_t *product_id, uint8_t *device_id);
static struct bt_conn_cb conn_callbacks;
// 制造商数据
static const uint8_t mfg_data[] = {
		0xAB, 0x0A, // 原始标识 (0xAB0A)
		0x8A, 0x9E, 0x4C, 0xD0, // UUID 前32位
		0xEB, 0xD8, // UUID 中间16位
		0x4E, 0x54, // UUID 中间16位
		0x8A, 0x6D, // UUID 中间16位
		0x5F, 0x0D, 0x13, 0x38, 0x97, 0x00 // UUID 后48位
};

// 配网响应分包结构体
struct ble_config_response_fragment {
	uint8_t idex; // 当前分片索引
	uint8_t cnt; // 总分片数
	uint8_t len; // 当前分片数据长度
	uint8_t data[0]; // 柔性数组
} __packed;

// GATT服务定义
BT_GATT_SERVICE_DEFINE(wifi_service, BT_GATT_PRIMARY_SERVICE(&wifi_service_uuid),
		BT_GATT_CHARACTERISTIC(
				&wifi_write_uuid.uuid, BT_GATT_CHRC_WRITE, BT_GATT_PERM_WRITE, NULL, write_wifi_data, NULL),
		BT_GATT_CHARACTERISTIC(&wifi_notify_uuid.uuid, BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_NONE, NULL, read_signed, NULL),
		BT_GATT_CCC(wifi_notify_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE));
// 广播数据定义
static const struct bt_data ad[] = {BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
		BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
		BT_DATA(BT_DATA_MANUFACTURER_DATA, mfg_data, sizeof(mfg_data))};

// 接收缓冲区
static uint8_t wifi_rx_buffer[512]; // 增大缓冲区
static uint16_t wifi_rx_size;
static bool is_receiving = false; // 标记是否正在接收数据

static void wifi_notify_changed(struct bt_gatt_attr *attr, uint16_t value)
{
	bool notif_enabled = (value == BT_GATT_CCC_NOTIFY);
	LOG_INF("Notifications %s\n", notif_enabled ? "enabled" : "disabled");
}

static struct bt_conn *current_conn = NULL; // 添加到文件开头的全局变量区

// 清理 JSON 字符串中的非打印字符
static void clean_json_string(char *str)
{
	char *src = str;
	char *dst = str;

	while (*src) {
		// 只保留可打印字符和空格
		if (*src >= 32 && *src <= 126) {
			*dst = *src;
			dst++;
		}
		src++;
	}
	*dst = '\0';
}
typedef struct {
	char chip_id[DEVICE_CHIP_ID_LENGTH];
	char product_id[DEVICE_PRODUCT_ID_LENGTH];
	char secret_id[DEVICE_SECRET_ID_LENGTH];
} device_info_t;
static int device_info_read(device_info_t *inf)
{
	chip_id_read(inf->chip_id, sizeof(inf->chip_id));
	if (0 > ls_cmd_nvs_get_value(LLM_CHAT_PRODUCT_ID, inf->product_id, sizeof(inf->product_id))) {
		snprintf(inf->product_id, sizeof(inf->product_id), "%s", CONFIG_PRODUCT_ID_STRING);
		LOG_WRN("Read product id failed,use default:%s", CONFIG_PRODUCT_ID_STRING);
	}

	if (0 > ls_cmd_nvs_get_value(LLM_CHAT_SECRET_ID, inf->secret_id, sizeof(inf->secret_id))) {
		snprintf(inf->secret_id, sizeof(inf->secret_id), "%s", CONFIG_SECRET_ID_STRING);
		LOG_WRN("Read secret id failed,use default:%s", CONFIG_SECRET_ID_STRING);
	}

	return 0;
}
static char ssid_buffer[64];
static char password_buffer[64];
static ssize_t write_wifi_data(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len,
		uint16_t offset, uint8_t flags)
{
	LOG_INF("Received data: len=%d, offset=%d, flags=%d\n", len, offset, flags);
	if (wifi_rx_size + len > sizeof(wifi_rx_buffer)) {
		// 缓冲区已满，重置
		wifi_rx_size = 0;
		is_receiving = false;
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	// 检查是否是新的传输开始
	if (!is_receiving) {
		const char *data = buf;
		if (len > 0 && data[0] == '{') {
			// 新的 JSON 开始
			wifi_rx_size = 0;
			memset(wifi_rx_buffer, 0, sizeof(wifi_rx_buffer));
			is_receiving = true;
			LOG_INF("Start receiving new JSON\n");
		}
	}

	// 追加数据到缓冲区
	memcpy(wifi_rx_buffer + wifi_rx_size, buf, len);
	LOG_INF("Append data: size=%d, len=%d, data=%.*s\n", wifi_rx_size, len, len, (const char *)buf);
	wifi_rx_size += len;

	// 临时添加结束符以便检查内容
	wifi_rx_buffer[wifi_rx_size] = '\0';
	LOG_INF("Current buffer: %s\n", wifi_rx_buffer);

	// 检查是否收到了完整的 JSON
	bool has_end = false;
	for (int i = 0; i < len; i++) {
		if (((const char *)buf)[i] == '}') {
			has_end = true;
			LOG_INF("Found JSON end marker\n");
			break;
		}
	}

	if (has_end) {
		wifi_rx_buffer[wifi_rx_size] = '\0';
		LOG_INF("Processing complete JSON: %s\n", wifi_rx_buffer);

		// 尝试解析 JSON
		char *start = strchr((char *)wifi_rx_buffer, '{');
		char *end = strrchr((char *)wifi_rx_buffer, '}');

		if (start && end && end > start) {
			size_t json_len = end - start + 1;
			char json_buffer[512];
			memcpy(json_buffer, start, json_len);
			json_buffer[json_len] = '\0';

			// 清理 JSON 字符串
			clean_json_string(json_buffer);
			LOG_INF("Cleaned JSON: %s\n", json_buffer);

			// 解析 JSON
			cJSON *root = cJSON_Parse(json_buffer);
			if (root) {

				// 获取 ssid 和 password 字段
				cJSON *ssid_item = cJSON_GetObjectItem(root, "ssid");
				cJSON *password_item = cJSON_GetObjectItem(root, "password");

				LOG_INF("JSON items - ssid_item: %p, password_item: %p\n", ssid_item, password_item);
				if (ssid_item) {
					LOG_INF("SSID item type: %d\n", ssid_item->type);
				}
				if (password_item) {
					LOG_INF("Password item type: %d\n", password_item->type);
				}

				// 检查 ssid 是否是字符串
				if (ssid_item && ssid_item->type == cJSON_String) {
					LOG_INF("SSID: %s\n", ssid_item->valuestring);
					strncpy(ssid_buffer, ssid_item->valuestring, sizeof(ssid_buffer) - 1);
					ssid_buffer[sizeof(ssid_buffer) - 1] = '\0';
				}

				// 检查 password 是否是字符串
				if (password_item && password_item->type == cJSON_String) {
					LOG_INF("Password: %s\n", password_item->valuestring);
					strncpy(password_buffer, password_item->valuestring, sizeof(password_buffer) - 1);
					password_buffer[sizeof(password_buffer) - 1] = '\0';
				}

				if (ssid_item && password_item) {
					LOG_INF("Complete WiFi config - SSID: %s, Password: %s\n", ssid_buffer, password_buffer);
					k_msleep(100);

					k_sem_give(&ble_sem);
					
					extern int wifi_connect_to_specified_ap_sync_by_ssid(const char *ssid,
							     const char *password);
					wifi_connect_to_specified_ap_sync_by_ssid(ssid_buffer,password_buffer);
				}

				// 打印整个 JSON 字符串用于调试
				char *json_str = cJSON_Print(root);
				if (json_str) {
					LOG_INF("Formatted JSON: %s\n", json_str);
					cJSON_free(json_str);
				}

				cJSON_Delete(root);
			} else {
				LOG_ERR("Failed to parse JSON: %s\n", cJSON_GetErrorPtr());
			}

			// 重置接收状态
			is_receiving = false;
			wifi_rx_size = 0;
		} else {
			LOG_ERR("Invalid JSON format: start=%p, end=%p\n", start, end);
		}
	}

	return len;
}

static int signed_value;
static ssize_t read_signed(
		struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset)
{
	int *value = &signed_value;
	LOG_INF("read_signed\n");
	return bt_gatt_attr_read(conn, attr, buf, len, offset, value, sizeof(signed_value));
}

int settings_runtime_load(void)
{
#if defined(CONFIG_BT_DIS_SETTINGS)
	settings_runtime_set("bt/dis/model", "Zephyr Model", sizeof("Zephyr Model"));
	settings_runtime_set("bt/dis/manuf", "Zephyr Manufacturer", sizeof("Zephyr Manufacturer"));
#if defined(CONFIG_BT_DIS_SERIAL_NUMBER)
	settings_runtime_set("bt/dis/serial", CONFIG_BT_DIS_SERIAL_NUMBER_STR, sizeof(CONFIG_BT_DIS_SERIAL_NUMBER_STR));
#endif
#if defined(CONFIG_BT_DIS_SW_REV)
	settings_runtime_set("bt/dis/sw", CONFIG_BT_DIS_SW_REV_STR, sizeof(CONFIG_BT_DIS_SW_REV_STR));
#endif
#if defined(CONFIG_BT_DIS_FW_REV)
	settings_runtime_set("bt/dis/fw", CONFIG_BT_DIS_FW_REV_STR, sizeof(CONFIG_BT_DIS_FW_REV_STR));
#endif
#if defined(CONFIG_BT_DIS_HW_REV)
	settings_runtime_set("bt/dis/hw", CONFIG_BT_DIS_HW_REV_STR, sizeof(CONFIG_BT_DIS_HW_REV_STR));
#endif
#endif
	return 0;
}

int ble_init(void)
{
	int err;

	LOG_INF("Starting Bluetooth initialization...\n");
	err = bt_enable(NULL);
	if (err) {
		LOG_INF("Bluetooth init failed (err %d)\n", err);
		return err;
	}

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		LOG_INF("Loading settings...\n");
		settings_load();
	}

	// 注册连接回调
	bt_conn_cb_register(&conn_callbacks);

	LOG_INF("Bluetooth initialized\n");
	return 0;
}

int ble_start_adv(void)
{
	app_player_start(TONE_PLAYER, "/lfs/100_ble_open.mp3");

	int err;

	// 开始广播
	LOG_INF("Starting advertising...\n");

	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		LOG_INF("Advertising failed to start (err %d)\n", err);
		return err;
	}

	LOG_INF("Advertising successfully started\n");
	return 0;
}
int ble_disconnect(void)
{
	int err;

	err = bt_conn_disconnect(current_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	if (err) {
		LOG_ERR("Failed to disconnect (err %d)\n", err);
		return err;
	}

	LOG_INF("Disconnected\n");
	return 0;
}
int ble_stop_adv(void)
{
	app_player_start(TONE_PLAYER, "/lfs/101_ble_close.mp3");

	int err;

	err = bt_le_adv_stop();
	if (err) {
		LOG_ERR("Failed to stop advertising (err %d)\n", err);
		return err;
	}

	LOG_INF("Advertising stopped\n");
	return 0;
}

static void mtu_exchange_cb(struct bt_conn *conn, uint8_t err, struct bt_gatt_exchange_params *params)
{
	if (!err) {
		LOG_INF("MTU exchange completed, MTU: %d", bt_gatt_get_mtu(conn));
	} else {
		LOG_WRN("MTU exchange failed (err %d)", err);
	}
}

static struct bt_gatt_exchange_params mtu_params = {
		.func = mtu_exchange_cb,
};

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_ERR("Connection failed (err %u)\n", err);
		return;
	}
	current_conn = bt_conn_ref(conn);
	LOG_INF("Connected\n");

	// 等待100ms后再进行MTU交换
	k_sleep(K_MSEC(100));

	// 设置MTU大小
	int ret = bt_gatt_exchange_mtu(conn, &mtu_params);
	if (ret) {
		LOG_WRN("MTU exchange failed (err %d)\n", ret);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("Disconnected (reason 0x%02x)\n", reason);

	if (current_conn) {
		bt_conn_unref(current_conn);
		current_conn = NULL;
	}

	// // 断开连接后重新开始广播
	// int err = ble_start_adv();
	// if (err) {
	// 	LOG_ERR("Failed to restart advertising after disconnect (err %d)\n", err);
	// }
}

static struct bt_conn_cb conn_callbacks = {
		.connected = connected,
		.disconnected = disconnected,
};

int custom_ble_config_datas_response(uint8_t code, uint8_t *product_id, uint8_t *device_id)
{
	int err = -1;

	// 使用cJSON组包响应数据
	cJSON *root = cJSON_CreateObject();
	if (!root) {
		LOG_ERR("Failed to create JSON object\n");
		return -ENOMEM;
	}

	// 添加必需字段
	cJSON_AddNumberToObject(root, "code", code);
	if (product_id) {
		cJSON_AddStringToObject(root, "product_id", (char *)product_id);
	}
	if (device_id) {
		cJSON_AddStringToObject(root, "device_id", (char *)device_id);
	}

	// 生成JSON字符串
	char *json_str = cJSON_PrintUnformatted(root);
	cJSON_Delete(root);

	if (!json_str) {
		LOG_ERR("Failed to create JSON string\n");
		return -ENOMEM;
	}

	uint16_t json_len = strlen(json_str);
	LOG_INF("Response JSON: %s, len: %u\n", json_str, json_len);

	// 开始分包发送
	uint8_t idex = 1;
	const uint8_t fragment_size = 17; // 每个分片的数据大小

	uint8_t cnt = json_len / fragment_size;
	if ((json_len % fragment_size) > 0) {
		cnt += 1;
	}

	// 分配分片缓冲区
	struct ble_config_response_fragment *fragment =
			csk_malloc(sizeof(struct ble_config_response_fragment) + fragment_size);
	if (!fragment) {
		cJSON_free(json_str);
		LOG_ERR("Failed to allocate fragment buffer\n");
		return -ENOMEM;
	}

	// 发送完整分片
	while (idex < cnt) {
		fragment->idex = idex;
		fragment->cnt = cnt;
		fragment->len = fragment_size;

		memcpy(fragment->data, json_str + (idex - 1) * fragment_size, fragment_size);

		// 打印分片数据
		char temp_str[18] = {0};
		memcpy(temp_str, fragment->data, fragment->len);
		// LOG_INF("Fragment %d/%d data: %s", idex, cnt, temp_str);

		if (current_conn) {
			err = bt_gatt_notify(current_conn, &wifi_service.attrs[3], fragment,
					fragment->len + sizeof(struct ble_config_response_fragment));

			if (err) {
				LOG_ERR("Failed to send fragment %d/%d, err %d\n", idex, cnt, err);
				break;
			}

			// LOG_INF("Sent fragment %d/%d, size %d\n", idex, cnt, fragment->len);
			k_sleep(K_MSEC(20));
		} else {
			LOG_ERR("No active connection\n");
			err = -EINVAL;
			break;
		}

		idex++;
	}

	// 发送最后一个不完整分片
	uint8_t remain = json_len % fragment_size;
	if (remain > 0 && err == 0) {
		fragment->idex = idex;
		fragment->cnt = cnt;
		fragment->len = remain;

		memcpy(fragment->data, json_str + (idex - 1) * fragment_size, remain);

		// 打印分片数据
		char temp_str[18] = {0};
		memcpy(temp_str, fragment->data, fragment->len);
		// LOG_INF("Last fragment %d/%d data: %s", idex, cnt, temp_str);

		if (current_conn) {
			err = bt_gatt_notify(current_conn, &wifi_service.attrs[3], fragment,
					fragment->len + sizeof(struct ble_config_response_fragment));

			if (err) {
				LOG_ERR("Failed to send last fragment %d/%d, err %d\n", idex, cnt, err);
			} else {
				// LOG_INF("Sent last fragment %d/%d, size %d\n", idex, cnt, fragment->len);
			}
		} else {
			LOG_ERR("No active connection\n");
			err = -EINVAL;
		}
	}

	// 清理资源
	csk_free(fragment);
	cJSON_free(json_str);

	if (err) {
		LOG_ERR("Failed to send response (err %d)\n", err);
		return err;
	}

	LOG_INF("Response sent successfully\n");
	return 0;
}

void ble_connect(void *p1, void *p2, void *p3)
{
	k_msleep(15000);
	// 初始化蓝牙
	int err = ble_init();
	if (err) {
		LOG_ERR("Failed to initialize BLE (err %d)\n", err);
		return;
	}
	device_info_t info;
	device_info_read(&info);
	LOG_INF("Initialization complete\n");
	while (1) {
		k_sem_take(&ble_sem, K_FOREVER);

		// extern int wait_wifi_sta_sem(void);
		// extern int get_wifi_sta_flg(void);

		// err = wait_wifi_sta_sem();

		// if (err) {
		// 	LOG_ERR("Failed to wait for WiFi connection (err %d)\n", err);
		// }
		k_sleep(K_MSEC(5000));
		// if ((get_wifi_sta_flg() == 1) && (err == 0)) {

			// 发送配置响应
			err = custom_ble_config_datas_response(1, (uint8_t *)info.product_id, (uint8_t *)info.chip_id);
			if (err) {
				LOG_ERR("Failed to send config response: %d\n", err);
			}
		// } else {
		// 	// 发送连接响应
		// 	err = custom_ble_config_datas_response(2, (uint8_t *)info.product_id, (uint8_t *)info.chip_id);
		// 	if (err) {
		// 		LOG_ERR("Failed to send connect response: %d\n", err);
		// 	}
		// 	// app_player_start(TONE_PLAYER, "/lfs/075_wifi_net_failed.mp3");
		// 	k_sleep(K_MSEC(5000));
		// }
		

		ble_stop_adv();
		k_sleep(K_MSEC(20)); /* sleep for 10 ms */
	}
}

int ble_connect_task_init(void)
{
	k_thread_stack_t *stack;
	k_tid_t tid;
	const int stack_size = 6 * 1024;
	const int thread_prio = 2;

	stack = csk_aligned_alloc(8, stack_size);
	if (stack == NULL) {
		LOG_ERR("app obj rec stack malloc failed");
		return -ENOMEM;
	}

	struct k_thread *new_thread = csk_malloc(sizeof(struct k_thread));
	if (new_thread == NULL) {
		csk_free(stack);
		return -ENOMEM;
	}

	tid = k_thread_create(new_thread, stack, stack_size, ble_connect, NULL, NULL, NULL, thread_prio, 0, K_NO_WAIT);
	if (tid != new_thread) {
		csk_free(new_thread);
		csk_free(stack);
		LOG_ERR("ble connect thread create failed: %p", tid);
		return -EFAULT;
	}

	k_thread_name_set(tid, "ble_connect");
	LOG_INF("ble connect thread created successfully");

	return 0;
}