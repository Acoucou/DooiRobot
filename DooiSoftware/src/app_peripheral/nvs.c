#include <stdbool.h>
#include <stdint.h>
#include <zephyr/fs/nvs.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/device.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sys_status, LOG_LEVEL_INF);

static struct nvs_fs fs;

/* ---------- NVS item IDs (16-bit) ---------- */
#define NVS_ID_INTERACTIVE_STATUS   0x0001  /* 对话模式：uint8_t */
#define NVS_ID_SERVO_OFFSET_L       0x0002  /* 舵机左偏移：int32_t */
#define NVS_ID_SERVO_OFFSET_R       0x0003  /* 舵机右偏移：int32_t */
#define NVS_ID_MOTOR_DIR_L          0x0004  /* 电机左方向：uint8_t */
#define NVS_ID_MOTOR_DIR_R          0x0005  /* 电机右方向：uint8_t */
#define NVS_ID_MIC_GAIN_PAIR        0x0006  /* 麦克风增益 int16_t a + int16_t d */

#define USER_NVS_PARTITION user_nvs_storage
#define USER_NVS_PARTITION_DEVICE FIXED_PARTITION_DEVICE(USER_NVS_PARTITION)
#define USER_NVS_PARTITION_OFFSET FIXED_PARTITION_OFFSET(USER_NVS_PARTITION)
#define USER_NVS_PARTITION_SIZE FIXED_PARTITION_SIZE(USER_NVS_PARTITION)

/* ---------- Types ---------- */
typedef struct {
    int8_t a;   /* wk_params.mic_gain_a_val */
    int8_t d;   /* wk_params.mic_gain_d_val */
} mic_gain_pair_t;


uint32_t user_nvs_init(void)
{
	int rc = 0;
	struct flash_pages_info info;
	fs.flash_device = USER_NVS_PARTITION_DEVICE;
	if (!device_is_ready(fs.flash_device)) {
		LOG_INF("Flash device %s is not ready\n", fs.flash_device->name);
		return -1;
	}
	fs.offset = USER_NVS_PARTITION_OFFSET;
	rc = flash_get_page_info_by_offs(fs.flash_device, fs.offset, &info);
	if (rc) {
		LOG_INF("Unable to get page info\n");
		return 0;
	}
	fs.sector_size = info.size;
	fs.sector_count = USER_NVS_PARTITION_SIZE / fs.sector_size;

	rc = nvs_mount(&fs);
	if (rc) {
		LOG_INF("user_nvs_storage init failed, erase & retry ..., ret: %d", rc);
		flash_erase(fs.flash_device, fs.offset, USER_NVS_PARTITION_SIZE);
		rc = nvs_mount(&fs);
		if (rc != 0) {
			LOG_INF("user_nvs_storage re-init failedret: %d", rc);
			return 0;
		}
	}
	return 0;
}

void user_nvs_write(uint8_t active)
{
	LOG_INF("user nvs write interactive: %d\n", active);
	nvs_write(&fs, NVS_ID_INTERACTIVE_STATUS, &active, sizeof(active));
}

void user_nvs_read(uint8_t *active)
{
	int rc = 0;
	rc = nvs_read(&fs, NVS_ID_INTERACTIVE_STATUS, active, sizeof(*active));
	if (rc < 0) {
		*active = 1;
	}
}

/* ----------------------------------------- */
/* 1. 舵机左右偏移：两个 int32_t              */
/* ----------------------------------------- */
int user_nvs_write_servo_offsets(int32_t left, int32_t right)
{
    int rc;

    rc = nvs_write(&fs, NVS_ID_SERVO_OFFSET_L, &left, sizeof(left));
    if (rc < 0) {
        LOG_INF("write SERVO_OFFSET_L failed rc=%d", rc);
        return rc;
    }

    rc = nvs_write(&fs, NVS_ID_SERVO_OFFSET_R, &right, sizeof(right));
    if (rc < 0) {
        LOG_INF("write SERVO_OFFSET_R failed rc=%d", rc);
        return rc;
    }

    LOG_INF("servo offsets saved: L=%ld, R=%ld", (long)left, (long)right);
    return 0;
}

int user_nvs_read_servo_offsets(int32_t *left, int32_t *right)
{
    int rcL = 0, rcR = 0;

    if (left) {
        rcL = nvs_read(&fs, NVS_ID_SERVO_OFFSET_L, left, sizeof(*left));
        if (rcL < 0) {
            *left = 0; /* 默认值 */
            LOG_INF("SERVO_OFFSET_L not found, default=%ld", (long)*left);
        }
    }

    if (right) {
        rcR = nvs_read(&fs, NVS_ID_SERVO_OFFSET_R, right, sizeof(*right));
        if (rcR < 0) {
            *right = 0; /* 默认值 */
            LOG_INF("SERVO_OFFSET_R not found, default=%ld", (long)*right);
        }
    }

    return (rcL < 0 || rcR < 0) ? -ENOENT : 0;
}

/* ----------------------------------------- */
/* 2. 电机左右方向：两个 uint8_t              */
/* ----------------------------------------- */
int user_nvs_write_motor_dirs(uint8_t left, uint8_t right)
{
    int rc;

    rc = nvs_write(&fs, NVS_ID_MOTOR_DIR_L, &left, sizeof(left));
    if (rc < 0) {
        LOG_INF("write MOTOR_DIR_L failed rc=%d", rc);
        return rc;
    }

    rc = nvs_write(&fs, NVS_ID_MOTOR_DIR_R, &right, sizeof(right));
    if (rc < 0) {
        LOG_INF("write MOTOR_DIR_R failed rc=%d", rc);
        return rc;
    }

    LOG_INF("motor dirs saved: L=%u, R=%u", left, right);
    return 0;
}

int user_nvs_read_motor_dirs(uint8_t *left, uint8_t *right)
{
    int rcL = 0, rcR = 0;

    if (left) {
        rcL = nvs_read(&fs, NVS_ID_MOTOR_DIR_L, left, sizeof(*left));
        if (rcL < 0) {
            *left = 0; /* 默认值 */
            LOG_INF("MOTOR_DIR_L not found, default=%u", *left);
        }
    }

    if (right) {
        rcR = nvs_read(&fs, NVS_ID_MOTOR_DIR_R, right, sizeof(*right));
        if (rcR < 0) {
            *right = 0; /* 默认值 */
            LOG_INF("MOTOR_DIR_R not found, default=%u", *right);
        }
    }

    return (rcL < 0 || rcR < 0) ? -ENOENT : 0;
}

/* ----------------------------------------- */
/* 麦克风增益成对：  */
/* ----------------------------------------- */
int user_nvs_write_mic_gains(int8_t a_val, int8_t d_val)
{
    mic_gain_pair_t pair = { .a = a_val, .d = d_val };
    int rc = nvs_write(&fs, NVS_ID_MIC_GAIN_PAIR, &pair, sizeof(pair));
    if (rc < 0) {
        LOG_INF("write MIC_GAIN_PAIR failed rc=%d", rc);
        return rc;
    }
    LOG_INF("mic gains saved: a=%d, d=%d", (int)a_val, (int)d_val);
    return 0;
}

int user_nvs_read_mic_gains(int8_t *a_val, int8_t *d_val)
{
    mic_gain_pair_t pair;
    int rc = nvs_read(&fs, NVS_ID_MIC_GAIN_PAIR, &pair, sizeof(pair));
    if (rc < 0) {
        /* 默认值：与需求一致 */
        if (a_val) *a_val = 25;
        if (d_val) *d_val = 0;
        LOG_INF("MIC_GAIN_PAIR not found, defaults a=25 d=0");
        return -ENOENT;
    }

    if (a_val) *a_val = pair.a;
    if (d_val) *d_val = pair.d;
    LOG_INF("mic gains loaded: a=%d, d=%d", (int)pair.a, (int)pair.d);
    return 0;
}

