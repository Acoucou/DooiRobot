/**
 * @file soc_adc.h
 * @brief ADC采样模块接口(通道2专用)
 */

#ifndef SOC_ADC_H
#define SOC_ADC_H

#include <zephyr/drivers/adc.h>
#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化ADC模块(通道2)
 * 
 * @return int 0表示成功，负数表示错误码
 */
int soc_adc_init(void);

/**
 * @brief 读取通道2的原始采样值
 * 
 * @param raw_value 输出原始采样值
 * @return int 0表示成功，负数表示错误码
 */
int soc_adc_read_raw(int16_t *raw_value);

/**
 * @brief 读取通道2的电压值(mV)
 * 
 * @param mv_value 输出电压值(mV)
 * @return int 0表示成功，负数表示错误码
 */
int soc_adc_read_mv(int32_t *mv_value);

int soc_adc_read_soc(uint8_t *soc_percent);

void soc_adc_enable(void);
void soc_adc_disable(void);

#ifdef __cplusplus
}
#endif

#endif /* SOC_ADC_H */
