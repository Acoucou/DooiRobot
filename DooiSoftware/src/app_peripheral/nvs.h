#ifndef NVS_H
#define NVS_H

#include <stdint.h>

/* ---------- Public APIs ---------- */
uint32_t user_nvs_init(void);

/* 3. 对话模式 */
void user_nvs_write(uint8_t active);
void user_nvs_read(uint8_t *active);

/* 1. 舵机左右偏移（int32_t） */
int user_nvs_write_servo_offsets(int32_t left, int32_t right);
int user_nvs_read_servo_offsets(int32_t *left, int32_t *right);

/* 2. 电机左右方向（uint8_t） */
int user_nvs_write_motor_dirs(uint8_t left, uint8_t right);
int user_nvs_read_motor_dirs(uint8_t *left, uint8_t *right);

int user_nvs_write_mic_gains(int8_t a_val, int8_t d_val);
int user_nvs_read_mic_gains(int8_t *a_val, int8_t *d_val);

#endif // NVS_H
