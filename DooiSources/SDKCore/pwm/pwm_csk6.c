/*
 * Copyright (c) 2021 listenai Systems (anhui) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT listenai_csk_pwm

#include <errno.h>

#include <soc.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include "Driver_GPT_PWM.h"
#include "venus_ap.h"
#include "cmn_sysctrl_reg_venus.h"
#include <zephyr/logging/log.h>
#include "gpt.h"

LOG_MODULE_REGISTER(pwm_csk6, CONFIG_PWM_LOG_LEVEL);

#define MAX_PWM_CHANNEL  8
#define MODE_TIMER  (0)
#define MODE_PWM  (1)


/** PWM configuration. */
struct pwm_csk_config {
	uint8_t clock_source;
	uint8_t clock_prescaler;
	uint8_t mode;
	uint8_t channel_mode;
	/* PWM alternate configuration */
    const struct pinctrl_dev_config *pcfg;
};

static uint32_t get_polarity(pwm_flags_t flags)
{
	if ((flags & PWM_POLARITY_MASK) == PWM_POLARITY_NORMAL) {
		return CSK_GPT_PWM_OUTPOLARITY_HIGH;
	}

	return CSK_GPT_PWM_OUTPOLARITY_LOW;
}

static inline uint32_t get_clock_source(uint8_t clock_source)
{

	return (3 - clock_source) << CSK_GPT_PWM_CLKSRC_Pos;
}

static inline uint32_t get_mode(uint8_t mode)
{
	return (mode - 1) << CSK_GPT_PWM_OUTMODE_Pos;
}

static uint32_t get_clock_prescaler(uint8_t clock_prescaler)
{
	uint8_t bit = 0;
	while(clock_prescaler){
		if (clock_prescaler & 0x01){
			break;
		}
		bit++;
		clock_prescaler = clock_prescaler >> 1;
	}
	return bit;
}

static int pwm_csk_get_cycles_per_sec(const struct device *dev,
					uint32_t pwm,
					uint64_t *cycles)
{
	const struct pwm_csk_config *cfg = dev->config;
	uint64_t clockfreq[4] = {0, PCLOCK, CLOCK_EXT, CLOCK_XTAL};
	*cycles = clockfreq[cfg->clock_source] >> get_clock_prescaler(cfg->clock_prescaler) ;
	return 0;
}

static int pwm_csk_pin_set(const struct device *dev, uint32_t pwm,
			     uint32_t period_cycles, uint32_t pulse_cycles,
			     pwm_flags_t flags)
{
	uint8_t duty_cycle;
	int32_t freq;
	uint64_t cycles_per_sec;
	int ret;
	const struct pwm_csk_config *cfg = dev->config;
	if (pwm > MAX_PWM_CHANNEL) {
		LOG_ERR("Invalid channel (%d)", pwm);
		return -EINVAL;
	}

	/* disable channel output if period is zero */
	if (period_cycles == 0U) {
		ret = HAL_GPT_DisablePWM(GPT0_PWM(), pwm);
		return (ret == CSK_DRIVER_OK) ? 0 : -EIO;
	}

	duty_cycle = 200 * pulse_cycles / period_cycles;
	/* period_cycles to period */
	pwm_csk_get_cycles_per_sec(dev, pwm, &cycles_per_sec);
	/* period to freq */
	freq = cycles_per_sec / period_cycles;

	HAL_GPT_PWMControl(GPT0_PWM(), CSK_GPT_PWM_MODE | get_clock_prescaler(cfg->clock_prescaler) << CSK_GPT_PWM_CLKDIV_Pos |
		get_clock_source(cfg->clock_source) | get_polarity(flags) | get_mode(cfg->mode) |
		CSK_GPT_PWM_OPERATION_MODE_PWM, pwm);
	ret = HAL_GPT_SetPWMFreqDuty(GPT0_PWM(), pwm, freq, duty_cycle);
	if (ret != CSK_DRIVER_OK){
		return -EIO;
	}
	// Note: When the PWM duty cycle is set to 0% or 100%, HAL_GPT_SetPWMFreq Duty does not reset the PWM reload value,
	// and directly disable the PWM output, so return here and cannot turn on the PWM.
	if((pulse_cycles == period_cycles) || (pulse_cycles ==  0u)) {
		return 0;
	}
	ret = HAL_GPT_EnablePWM(GPT0_PWM(), pwm);
	if (ret != CSK_DRIVER_OK){
		return -EIO;
	}
	return 0;
}


static const struct pwm_driver_api pwm_csk_driver_api = {
	.set_cycles = pwm_csk_pin_set,
	.get_cycles_per_sec = pwm_csk_get_cycles_per_sec,
};


static int pwm_csk_init(const struct device *dev)
{
	const struct pwm_csk_config *config = dev->config;
	if (config->channel_mode != MODE_PWM) {
		return -EPERM;
	}
	HAL_GPT_PWMInitialize(GPT0_PWM(), NULL);
	HAL_GPT_PWMPowerControl(GPT0_PWM(), CSK_POWER_FULL);

	pinctrl_apply_state(config->pcfg, PINCTRL_STATE_DEFAULT);
	return 0;
}

#if !defined(CONFIG_COUNTER_CSK6_GPTCHANNEL)
static void csk_pwm_isr(void)
{
	extern void gpt0_irq_handler();

	gpt0_irq_handler();
}

static int csk_gpt_init(const struct device *dev)
{
	IRQ_CONNECT(DT_IRQN(DT_INST(0, listenai_csk_gpt)),
		    DT_IRQ(DT_INST(0, listenai_csk_gpt), priority), csk_pwm_isr,
		    DEVICE_DT_GET(DT_INST(0, listenai_csk_gpt)), 0);

	irq_enable(DT_IRQN(DT_INST(0, listenai_csk_gpt)));

	return 0;
}
#endif

DEVICE_DT_DEFINE(DT_INST(0, listenai_csk_gpt), csk_gpt_init, NULL, NULL, NULL, PRE_KERNEL_1,
		 CONFIG_KERNEL_INIT_PRIORITY_DEVICE, NULL);

#define PWM_DEVICE_INIT(index)                                                                     \
                                                                                                   \
	PINCTRL_DT_INST_DEFINE(index);                                                             \
                                                                                                   \
	static const struct pwm_csk_config pwm_csk_config_##index = {                              \
		.clock_source = DT_INST_PROP(index, clock_source),                                 \
		.clock_prescaler = DT_INST_PROP(index, clock_prescaler),                           \
		.mode = DT_INST_PROP(index, mode),                                                 \
		.channel_mode = DT_INST_PROP(index, channel_mode),                                 \
		.pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(index),                                     \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(index, &pwm_csk_init, NULL, NULL, &pwm_csk_config_##index,           \
			      POST_KERNEL, CONFIG_PWM_CSK6_INIT_PRIORITY, &pwm_csk_driver_api);

DT_INST_FOREACH_STATUS_OKAY(PWM_DEVICE_INIT)
