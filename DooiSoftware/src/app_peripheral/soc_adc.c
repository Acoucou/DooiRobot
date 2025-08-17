/**
 * @file soc_adc.c
 * @brief ADC采样模块实现(通道2专用)
 */

#include "soc_adc.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <math.h>

#if !DT_NODE_EXISTS(DT_PATH(zephyr_user)) || !DT_NODE_HAS_PROP(DT_PATH(zephyr_user), io_channels)
#error "No suitable devicetree overlay specified"
#endif

#define EXGPIOC_1_NODE_D DT_ALIAS(socadc)
static const struct gpio_dt_spec gpio_exc1 = GPIO_DT_SPEC_GET(EXGPIOC_1_NODE_D, gpios);

/* ADC配置参数 */
#define ADC_RESOLUTION 10
#define ADC_GAIN       ADC_GAIN_1
#define ADC_REFERENCE  ADC_REF_INTERNAL

#define ADC_RATIO      3.7


/* 静态全局变量 */
static const struct device *adc_dev;
static int32_t adc_vref;
static uint8_t channel_id;

/* 采样缓冲区 */
static int16_t sample_buffer;

/* ADC序列配置 */
static struct adc_sequence sequence = {
    .channels = 0,  // 将在初始化时设置
    .buffer = &sample_buffer,
    .buffer_size = sizeof(sample_buffer),
    .resolution = ADC_RESOLUTION,
};

/* 通道配置 */
static struct adc_channel_cfg channel_cfg = {
    .gain = ADC_GAIN,
    .reference = ADC_REFERENCE,
    .channel_id = 0, // 将在初始化时设置
};

typedef struct {
    int32_t full_charge_voltage;  // 满电电压 (mV)
    int32_t empty_voltage;        // 空电电压 (mV)
    float voltage_soc_curve[21];  // 电压-SOC曲线 (0%-100%每5%一个点)
} battery_params_t;

static const battery_params_t battery_params = {
    .full_charge_voltage = 4200,
    .empty_voltage = 3000,
    .voltage_soc_curve = 
    {
        3.00f, 3.40f, 3.60f, 3.70f, 3.75f, 
        3.78f, 3.81f, 3.83f, 3.85f, 3.87f, 
        3.89f, 3.91f, 3.94f, 3.97f, 4.00f, 
        4.04f, 4.08f, 4.12f, 4.16f, 4.18f, 
        4.20f
    }
};

// 计算SOC百分比范围 (0-100)
static inline uint8_t clamp_percent(float value) {
    if (value < 0) return 0;
    if (value > 100) return 100;
    return (uint8_t)value;
}

int soc_adc_calculate_soc(int32_t voltage_mv, uint8_t *soc_percent)
{
    const float voltage_v = voltage_mv / 1000.0f;
    const float *curve = battery_params.voltage_soc_curve;
    const size_t curve_size = sizeof(battery_params.voltage_soc_curve) / sizeof(float);
    
    // 边界检查
    if (voltage_v <= curve[0]) {
        *soc_percent = 0;
        return 0;
    }
    if (voltage_v >= curve[curve_size - 1]) {
        *soc_percent = 100;
        return 0;
    }

    // 查找相邻点
    size_t index = 1;
    while (index < curve_size && voltage_v > curve[index]) {
        index++;
    }

    // 线性插值计算SOC
    const float lower_volt = curve[index - 1];
    const float upper_volt = curve[index];
    const float segment_range = upper_volt - lower_volt;
    
    if (fabsf(segment_range) < 1e-6) {
        *soc_percent = (index - 1) * 5;
    } else {
        const float ratio = (voltage_v - lower_volt) / segment_range;
        *soc_percent = clamp_percent((index - 1) * 5 + ratio * 5);
    }
    
    return 0;
}

int soc_adc_read_soc(uint8_t *soc_percent)
{
    int32_t voltage_mv;
    int ret = soc_adc_read_mv(&voltage_mv);
    if (ret != 0) {
        return ret;
    }
    return soc_adc_calculate_soc(voltage_mv, soc_percent);
}

int soc_adc_init(void)
{
    /* 获取ADC设备 */
    adc_dev = DEVICE_DT_GET(DT_IO_CHANNELS_CTLR(DT_PATH(zephyr_user)));
    if (!device_is_ready(adc_dev)) {
        printk("ADC device not ready\n");
        return -ENODEV;
    }

    /* 获取通道ID */
    channel_id = DT_IO_CHANNELS_INPUT(DT_PATH(zephyr_user));
    
    /* 配置通道2 */
    channel_cfg.channel_id = channel_id;
    int err = adc_channel_setup(adc_dev, &channel_cfg);
    if (err) {
        printk("Failed to setup ADC channel 2 (err %d)\n", err);
        return err;
    }
    
    /* 设置通道掩码 */
    sequence.channels = BIT(channel_id);

    /* 获取参考电压 */
    adc_vref = adc_ref_internal(adc_dev);
    if (adc_vref <= 0) {
        printk("Invalid ADC reference voltage %d\n", adc_vref);
        return -EINVAL;
    }

    /* 初始化控制GPIO */
    if (!device_is_ready(gpio_exc1.port)) {
        printk("GPIO device not ready\n");
        return -ENODEV;
    }
    gpio_pin_configure_dt(&gpio_exc1, GPIO_OUTPUT_LOW);

    printk("ADC initialized. Channel: %d, Vref: %d mV\n", 
           channel_id, adc_vref);
    return 0;
}

int soc_adc_read_raw(int16_t *raw_value)
{
    int err = adc_read(adc_dev, &sequence);
    if (err) {
        printk("ADC read failed (err %d)\n", err);
        return err;
    }
    printk("ADC raw value: %d\n", sample_buffer);
    *raw_value = sample_buffer;
    return 0;
}

int soc_adc_read_mv(int32_t *mv_value)
{
    int16_t raw_value;
    int err = soc_adc_read_raw(&raw_value);
    if (err) {
        return err;
    }
    
    *mv_value = (int32_t)raw_value;
    adc_raw_to_millivolts(adc_vref, ADC_GAIN, ADC_RESOLUTION, mv_value);
    *mv_value *= ADC_RATIO;
    printk("ADC voltage: %d mV\n", *mv_value);
    return 0;
}

void soc_adc_enable(void)
{
    gpio_pin_set_dt(&gpio_exc1, 1);
}

void soc_adc_disable(void)
{
    gpio_pin_set_dt(&gpio_exc1, 0);
}
