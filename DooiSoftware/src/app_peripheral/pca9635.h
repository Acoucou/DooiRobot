/* pca9635_direct.h */
#ifndef PCA9635_DIRECT_H
#define PCA9635_DIRECT_H

#include "Driver_I2C.h"
#include "i2c.h"
#include <zephyr/drivers/i2c.h>

/* I2C地址 */
#define I2C_SLAVE_ADDR 0x00 // 确认是否为7位地址

/* 寄存器地址 */
#define PCA9635_MODE1        0x00
#define PCA9635_MODE2        0x01
#define PCA9635_PWM_BASE     0x02
#define PCA9635_GRPPWM       0x12
#define PCA9635_GRPFREQ      0x13
#define PCA9635_LEDOUT_BASE  0x14
#define PCA9635_SUBADR1      0x18
#define PCA9635_SUBADR2      0x19
#define PCA9635_SUBADR3      0x1A
#define PCA9635_ALLCALLADDR  0x1B

/* 模式寄存器配置 */
#define MODE1_AI2           BIT(7)  // 自动递增模式
#define MODE1_SLEEP         BIT(4)  // 睡眠模式
#define MODE2_DMBLNK        BIT(5)  // 组闪烁模式
#define MODE2_OUTDRV        BIT(2)  // 推挽输出


/* LED输出模式 */
typedef enum {
    PCA9635_PWM_OFF  = 0x00,  // 关闭
    PCA9635_PWM_ON   = 0x01,  // 常亮
    PCA9635_PWM_PWM  = 0x02,  // 独立PWM
    PCA9635_PWM_GRP  = 0x03   // PWM+组控制
} pca9635_pwm_mode_t;

int pca9635_init();
int pca9635_set_pwm(uint8_t channel, uint8_t duty);
int pca9635_set_pwm_mode( uint8_t channel, pca9635_pwm_mode_t mode);
int pca9635_set_pwms(uint8_t channel, uint8_t *duty, uint8_t len);

#endif /* PCA9635_DIRECT_H */
