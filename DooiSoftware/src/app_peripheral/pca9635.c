
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <stdint.h>
#include "pca9635.h"

// #include <zephyr/logging/log.h>
// LOG_MODULE_REGISTER(pca9635, CONFIG_LED_LOG_LEVEL);

/* I2C设备 */
#define I2C_INST DT_NODELABEL(i2c1)
static const struct device *const i2c_dev = DEVICE_DT_GET(I2C_INST);

/* 写入寄存器序列 */
static int pca9635_reg_write(uint8_t reg, uint8_t *data, uint32_t len)
{
    uint8_t buf[len + 1];
    
    buf[0] = reg;
    memcpy(&buf[1], data, len);
    
    return i2c_write(i2c_dev, buf, sizeof(buf), I2C_SLAVE_ADDR);
}

/* 初始化设备 */
int pca9635_init()
{
    int ret = 0;
    uint8_t mode = 0;

    /* 退出睡眠模式 */
    mode = MODE1_AI2; // 启用自动递增
    ret = pca9635_reg_write(PCA9635_MODE1, &mode, 1);

    /* 配置MODE2 */
    mode = MODE2_OUTDRV;
    ret = pca9635_reg_write(PCA9635_MODE2, &mode, 1);

    // 将所有通道设置为PWM模式
    uint8_t pwm_mode = 0x00; // 初始值为0
    pwm_mode |= (0x2 << 0); // 通道0: PWM模式 (10)
    pwm_mode |= (0x2 << 2); // 通道1: PWM模式 (10)
    pwm_mode |= (0x2 << 4); // 通道2: PWM模式 (10)
    pwm_mode |= (0x2 << 6); // 通道3: PWM模式 (10)
    ret = pca9635_reg_write(PCA9635_LEDOUT_BASE + 0, &pwm_mode, 1); // 写入LEDOUT0

    pwm_mode = 0x00; 
    pwm_mode |= (0x2 << 0); // 通道4: PWM模式 (10)
    pwm_mode |= (0x2 << 2); // 通道5: PWM模式 (10)
    ret = pca9635_reg_write(PCA9635_LEDOUT_BASE + 1, &pwm_mode, 1); // 写入LEDOUT1

    pwm_mode = 0x00; 
    pwm_mode |= (0x2 << 0); // 通道12: PWM模式 (10)
    pwm_mode |= (0x2 << 2); // 通道13: PWM模式 (10)
    pwm_mode |= (0x2 << 4); // 通道14: PWM模式 (10)
    pwm_mode |= (0x2 << 6); // 通道15: PWM模式 (10)
    ret = pca9635_reg_write(PCA9635_LEDOUT_BASE + 3, &pwm_mode, 1); // 写入LEDOUT3
    
    return ret;
}

/* 设置单个通道PWM */
int pca9635_set_pwm(uint8_t channel, uint8_t duty)
{ 
    return pca9635_reg_write(PCA9635_PWM_BASE + channel, &duty, 1);
}

int pca9635_set_pwms(uint8_t channel, uint8_t *duty, uint8_t len)
{ 
    return pca9635_reg_write(PCA9635_PWM_BASE + channel, duty, len);
}

/* 配置组控制参数 */
int pca9635_set_group(uint8_t duty, uint16_t freq)
{
    int ret;
    uint8_t data[2];
    
    /* 设置GRPPWM */
    data[0] = duty;
    ret = pca9635_reg_write(PCA9635_GRPPWM, data, 1);
    if (ret) return ret;
    
    /* 设置GRPFREQ */
    data[0] = (uint8_t)((freq >> 8) & 0xFF);
    data[1] = (uint8_t)(freq & 0xFF);
    return pca9635_reg_write(PCA9635_GRPFREQ, data, 2);
}

/* 设置PWM通道工作模式 */
int pca9635_set_pwm_mode( uint8_t channel, pca9635_pwm_mode_t mode)
{
	uint8_t reg_index = channel / 4;
	uint8_t shift = (channel % 4) * 2;

    uint8_t ledout[4]; 

	ledout[reg_index] &= ~(0x03 << shift);
	ledout[reg_index] |= (mode << shift);

	return pca9635_reg_write(PCA9635_LEDOUT_BASE + reg_index, ledout[reg_index], 1);
}

