/*
 * Copyright (c) 2017 Jan Van Winkel <jan.van_winkel@dxplore.eu>
 * Copyright (c) 2019 Nordic Semiconductor ASA
 * Copyright (c) 2019 Marc Reilly
 * Copyright (c) 2019 PHYTEC Messtechnik GmbH
 * Copyright (c) 2020 Endian Technologies AB
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define DT_DRV_COMPAT galaxyc_dual_gc9a01

#include "display_dual_gc9a01_csk6.h"
#include "display_csk6_lcd.h"

#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(display_dual_gc9a01, 4);

struct gc9a01_config {
    struct spi_dt_spec bus;
    struct gpio_dt_spec cmd_data_gpio;
    struct gpio_dt_spec reset_gpio;
    struct gpio_dt_spec two_lcd_sel_gpio;
    uint8_t mdac;
    uint8_t colmod;
    uint16_t width;
    uint16_t height;
    uint8_t rotation;
};

struct gc9a01_data {
	  uint16_t x_offset;
	  uint16_t y_offset;
    void *spi;
};

static void gc9a01_set_lcd_margins(const struct device *dev,
                                   uint16_t x_offset, uint16_t y_offset)
{
    struct gc9a01_data *data = dev->data;

    data->x_offset = x_offset;
    data->y_offset = y_offset;
}

__attribute__((section(".itcm"))) static int32_t gc9a01_transmit(const struct device *dev, uint8_t cmd,
                               uint8_t *tx_data, size_t tx_len)
{
    const struct gc9a01_config *config = dev->config;
    int r;
    struct gc9a01_data *data = dev->data;

    gpio_pin_set_dt(&config->cmd_data_gpio, 1);

    r = display_csk6_lcd_cmd_datas_send(data->spi, &cmd, 1);
    if (r != 0) {
        return r;
    }

    if ((tx_data != NULL) && (tx_len > 0)) {
        gpio_pin_set_dt(&config->cmd_data_gpio, 0);
        r = display_csk6_lcd_cmd_datas_send(data->spi, tx_data, tx_len);
        if (r != 0) {
            LOG_ERR("SPI write failed, error code:%d", r);
            return r;
        }
    }

    return 0;
}

static void gc9a01_exit_sleep(const struct device *dev)
{
    gc9a01_transmit(dev, GC9A01_CMD_SLPOUT, NULL, 0);
    k_sleep(K_MSEC(120));
    gc9a01_transmit(dev, GC9A01_CMD_DISPON, NULL, 0);
}

static void gc9a01_enter_sleep(const struct device *dev)
{
    gc9a01_transmit(dev, GC9A01_CMD_DISPOFF, NULL, 0);
    k_sleep(K_MSEC(120));
    gc9a01_transmit(dev, GC9A01_CMD_SLPIN, NULL, 0);
    k_sleep(K_MSEC(50));
}

static void gc9a01_reset_display(const struct device *dev)
{
    LOG_DBG("Resetting display");

    const struct gc9a01_config *config = dev->config;
    if (config->reset_gpio.port != NULL) {
        k_sleep(K_MSEC(1));
        gpio_pin_set_dt(&config->reset_gpio, 1);
        k_sleep(K_MSEC(50));
        gpio_pin_set_dt(&config->reset_gpio, 0);
        k_sleep(K_MSEC(120));
    } else {
        gc9a01_transmit(dev, GC9A01_CMD_SWRESET, NULL, 0);
        k_sleep(K_MSEC(5));
    }
}

static int gc9a01_blanking_on(const struct device *dev)
{
    gc9a01_transmit(dev, GC9A01_CMD_DISPOFF, NULL, 0);
    return 0;
}

static int gc9a01_blanking_off(const struct device *dev)
{
    gc9a01_transmit(dev, GC9A01_CMD_DISPON, NULL, 0);
    return 0;
}

static int gc9a01_read(const struct device *dev,
                      const uint16_t x,
                      const uint16_t y,
                      const struct display_buffer_descriptor *desc,
                      void *buf)
{
    return -ENOTSUP;
}

static void gc9a01_set_mem_area(const struct device *dev, const uint16_t x,
                                const uint16_t y, const uint16_t w, const uint16_t h)
{
    struct gc9a01_data *data = dev->data;
    uint16_t spi_data[2];

    uint16_t ram_x = x + data->x_offset;
    uint16_t ram_y = y + data->y_offset;

    spi_data[0] = sys_cpu_to_be16(ram_x);
    spi_data[1] = sys_cpu_to_be16(ram_x + w - 1);
    gc9a01_transmit(dev, GC9A01_CMD_CASET, (uint8_t *)&spi_data[0], 4);

    spi_data[0] = sys_cpu_to_be16(ram_y);
    spi_data[1] = sys_cpu_to_be16(ram_y + h - 1);
    gc9a01_transmit(dev, GC9A01_CMD_RASET, (uint8_t *)&spi_data[0], 4);
}

__attribute__((section(".itcm"))) static int gc9a01_write(const struct device *dev, const uint16_t x, const uint16_t y,
                        const struct display_buffer_descriptor *desc, const void *buf)
{
  const struct gc9a01_config *config = dev->config;
  struct gc9a01_data *data = dev->data;
  uint32_t new_h = desc->height;
  uint32_t new_w = desc->width;
  uint32_t new_x = x;
  uint32_t new_y = y;
    
#if CONFIG_gc9a01_CSK6_ROTATE_90
	new_h = desc->width;
	new_w = desc->height;
	new_y = x;
	new_x = config->width - (y + desc->height);
#endif

  gc9a01_set_mem_area(dev, new_x, new_y, new_w, new_h);
  gc9a01_transmit(dev, GC9A01_CMD_RAMWR, NULL, 0);
  gpio_pin_set_dt(&config->cmd_data_gpio, 0);

int r = display_csk6_lcd_display_datas_send(data->spi, buf, desc->height, desc->width,
					    IS_ENABLED(CONFIG_gc9a01_CSK6_ROTATE_90));
  if (r) {
      LOG_ERR("SPI write failed, r:%d", r);
  }

  return r;
}

static void *gc9a01_get_framebuffer(const struct device *dev)
{
    return NULL;
}

static int gc9a01_set_brightness(const struct device *dev,
                                 const uint8_t brightness)
{
    return -ENOTSUP;
}

static int gc9a01_set_contrast(const struct device *dev,
                               const uint8_t contrast)
{
    return -ENOTSUP;
}

static void gc9a01_get_capabilities(const struct device *dev,
                                    struct display_capabilities *capabilities)
{
    const struct gc9a01_config *config = dev->config;

    memset(capabilities, 0, sizeof(struct display_capabilities));
    capabilities->x_resolution = config->width;
    capabilities->y_resolution = config->height;

    capabilities->supported_pixel_formats = PIXEL_FORMAT_RGB_565;
    capabilities->current_pixel_format = PIXEL_FORMAT_RGB_565;
    capabilities->current_orientation = DISPLAY_ORIENTATION_NORMAL;
}

static int gc9a01_set_pixel_format(const struct device *dev,
                                   const enum display_pixel_format pixel_format)
{
    if (pixel_format == PIXEL_FORMAT_RGB_565) {
        return 0;
    }
    LOG_ERR("Pixel format change not implemented");
    return -ENOTSUP;
}

static int gc9a01_set_orientation(const struct device *dev,
                                  const enum display_orientation orientation)
{
    if (orientation == DISPLAY_ORIENTATION_NORMAL) {
        return 0;
    }
    LOG_ERR("Changing display orientation not implemented");
    return -ENOTSUP;
}

static void gc9a01_lcd_init(const struct device *dev)
{
    struct gc9a01_data *data = dev->data;
    const struct gc9a01_config *config = dev->config;
    uint8_t tmp;

    LOG_INF("gc9a01_lcd_init");
    
    gc9a01_set_lcd_margins(dev, data->x_offset, data->y_offset);
    
    gc9a01_transmit(dev, GC9A01_CMD_INREGEN1, NULL, 0);      //oxfe
    gc9a01_transmit(dev, GC9A01_CMD_INREGEN2, NULL, 0);      //oxef
    
    gc9a01_transmit(dev, 0xEB, (uint8_t[]){0x14}, 1);
    
    gc9a01_transmit(dev, 0x84, (uint8_t[]){0x40}, 1);
    gc9a01_transmit(dev, 0x85, (uint8_t[]){0xFF}, 1);
    gc9a01_transmit(dev, 0x86, (uint8_t[]){0xFF}, 1);
    gc9a01_transmit(dev, 0x87, (uint8_t[]){0xFF}, 1);
    gc9a01_transmit(dev, 0x88, (uint8_t[]){0x0A}, 1);
    gc9a01_transmit(dev, 0x89, (uint8_t[]){0x21}, 1);
    gc9a01_transmit(dev, 0x8a, (uint8_t[]){0x00}, 1);
    gc9a01_transmit(dev, 0x8b, (uint8_t[]){0x80}, 1);
    gc9a01_transmit(dev, 0x8c, (uint8_t[]){0x01}, 1);
    gc9a01_transmit(dev, 0x8d, (uint8_t[]){0x01}, 1);
    gc9a01_transmit(dev, 0x8e, (uint8_t[]){0xFF}, 1);
    gc9a01_transmit(dev, 0x8f, (uint8_t[]){0xFF}, 1);
    
    /* Display Function Control */
    gc9a01_transmit(dev, GC9A01_CMD_DISFUNCTRL, (uint8_t[]){0x00, 0x20}, 2);          //0xb6    

   	/* Memory Data Access Control */
    tmp = config->mdac;
    gc9a01_transmit(dev, GC9A01_CMD_MADCTL, &tmp, 1);          //0x36
        
    /* Interface Pixel Format */
    tmp = config->colmod;
    gc9a01_transmit(dev, GC9A01_CMD_COLMOD, &tmp, 1);            //0x3a
    
    gc9a01_transmit(dev, 0x90, (uint8_t[]){0x08, 0x08, 0x08, 0x08}, 4);
    
    
    gc9a01_transmit(dev, 0xBD, (uint8_t[]){0x06}, 1);
    gc9a01_transmit(dev, 0xBC, (uint8_t[]){0x00}, 1);
    
    gc9a01_transmit(dev, 0xFF, (uint8_t[]){0x60, 0x01, 0x04}, 3);    
    
    gc9a01_transmit(dev, GC9A01_CMD_PWCTRL2, (uint8_t[]){0x13}, 1);            //0xc3
    gc9a01_transmit(dev, GC9A01_CMD_PWCTRL3, (uint8_t[]){0x13}, 1);            //0xc4
    gc9a01_transmit(dev, GC9A01_CMD_PWCTRL4, (uint8_t[]){0x22}, 1);            //0xc9    
    
    gc9a01_transmit(dev, 0xBE, (uint8_t[]){0x11}, 1);
    gc9a01_transmit(dev, 0xE1, (uint8_t[]){0x10, 0x0E}, 2);
    gc9a01_transmit(dev, 0xDF, (uint8_t[]){0x21, 0x0C, 0x02}, 3);
    

    gc9a01_transmit(dev, GC9A01_CMD_GAM1SET, (uint8_t[]){0x45, 0x09, 0x08, 0x08, 0x26, 0x2A}, 6);        //0xf0
    gc9a01_transmit(dev, GC9A01_CMD_GAM2SET, (uint8_t[]){0x43, 0x70, 0x72, 0x36, 0x37, 0x6e}, 6);        //0xf1    
    gc9a01_transmit(dev, GC9A01_CMD_GAM3SET, (uint8_t[]){0x45, 0x09, 0x08, 0x08, 0x26, 0x2A}, 6);        //0xf2
    gc9a01_transmit(dev, GC9A01_CMD_GAM4SET, (uint8_t[]){0x43, 0x70, 0x72, 0x36, 0x37, 0x6f}, 6);        //0xf3
        
    gc9a01_transmit(dev, 0xED, (uint8_t[]){0x1B, 0x0B}, 2);
    gc9a01_transmit(dev, 0xAE, (uint8_t[]){0x77}, 1);
    gc9a01_transmit(dev, 0xCD, (uint8_t[]){0x63}, 1);
    gc9a01_transmit(dev, 0x70, (uint8_t[]){0x07, 0x07, 0x04, 0x0E, 0x0F, 0x09, 0x07, 0x08, 0x03}, 9);    
    
    /* Frame Rate */
    gc9a01_transmit(dev, GC9A01_CMD_FRAMERATE, (uint8_t[]){0x34}, 1);             //0xe8
        
    gc9a01_transmit(dev, 0x62, (uint8_t[]){0x18, 0x0D, 0x71, 0xED, 0x70, 0x70, 0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70}, 12);
    gc9a01_transmit(dev, 0x63, (uint8_t[]){0x18, 0x11, 0x71, 0xF1, 0xF0, 0x70, 0x18, 0x13, 0x71, 0xF3, 0x70, 0x70}, 12);
    gc9a01_transmit(dev, 0x64, (uint8_t[]){0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07}, 7);
    gc9a01_transmit(dev, 0x66, (uint8_t[]){0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45, 0x10, 0x00, 0x00, 0x00}, 10);
    gc9a01_transmit(dev, 0x67, (uint8_t[]){0x00, 0x3C, 0x00, 0x00, 0x00, 0x01, 0x54, 0x10, 0x32, 0x98}, 10);
    
     gc9a01_transmit(dev, 0x74, (uint8_t[]){0x10, 0x85, 0x80, 0x00, 0x00, 0x4E, 0x00}, 7);
        
     gc9a01_transmit(dev, 0x98, (uint8_t[]){0x3e, 0x07}, 2);   
    
    /* Tearing Effect Line Off */
    gc9a01_transmit(dev, GC9A01_CMD_TEOFF, NULL, 0);           //0x34
    /* Display Inversion ON */
    gc9a01_transmit(dev, GC9A01_CMD_INVON, NULL, 0);           //ox21
    LOG_INF("gc9a01_lcd_init done");
}

static int gc9a01_init(const struct device *dev)
{

    const struct gc9a01_config *config = dev->config;
    struct gc9a01_data *data = dev->data;

    LOG_INF("gc9a01_init");
    if (!device_is_ready(config->bus.bus)) {
        LOG_ERR("SPI device not ready");
        return -ENODEV;
    } else {
		data->spi = display_csk6_lcd_init(config->bus.bus->name,
						  CONFIG_GC9A01_CSK6_DMA_CHANNEL);
        if (data->spi == NULL) {
            LOG_ERR("HAL SPI device is null");
            k_panic();
        }
    }
 

    if (config->reset_gpio.port != NULL) {
      if (!device_is_ready(config->reset_gpio.port)) {
        LOG_ERR("Reset GPIO device not ready");
        return -ENODEV;
      }

      if (gpio_pin_configure_dt(&config->reset_gpio, GPIO_OUTPUT_INACTIVE)) {
        LOG_ERR("Couldn't configure reset pin");
        return -EIO;
      }
    }

    if (config->two_lcd_sel_gpio.port != NULL) {
      if (!device_is_ready(config->two_lcd_sel_gpio.port)) {
        LOG_ERR("two lcd sel GPIO device not ready");
        return -ENODEV;
      }

      if (gpio_pin_configure_dt(&config->two_lcd_sel_gpio, GPIO_OUTPUT_INACTIVE)) {
        LOG_ERR("Couldn't configure two lcd sel pin");
        return -EIO;
      }
    }
    
    if (!device_is_ready(config->cmd_data_gpio.port)) {
        LOG_ERR("CMD/DATA GPIO device not ready");
        return -ENODEV;
    }

    if (gpio_pin_configure_dt(&config->cmd_data_gpio, GPIO_OUTPUT)) {
        LOG_ERR("Couldn't configure CMD/DATA pin");
        return -EIO;
    }
    
    

    gc9a01_reset_display(dev);
    
    if (config->two_lcd_sel_gpio.port != NULL) {
    
        /* Initialize the first LCD panel */
        gpio_pin_set_dt(&config->two_lcd_sel_gpio, 0);
         
        k_sleep(K_MSEC(10));
         
        gc9a01_blanking_on(dev);
    
        gc9a01_lcd_init(dev);
    
        gc9a01_exit_sleep(dev);
        
        /* Initialize the second LCD panel */
        gpio_pin_set_dt(&config->two_lcd_sel_gpio, 1);
         
        k_sleep(K_MSEC(10));  
        
        gc9a01_blanking_on(dev);
    
        gc9a01_lcd_init(dev);
    
        gc9a01_exit_sleep(dev);   
        
    }        

    LOG_INF("gc9a01_init done");
    return 0;
}

#ifdef CONFIG_PM_DEVICE
static int gc9a01_pm_action(const struct device *dev,
                            enum pm_device_action action)
{
    int ret = 0;

    switch (action) {
    case PM_DEVICE_ACTION_RESUME:
        gc9a01_exit_sleep(dev);
        break;
    case PM_DEVICE_ACTION_SUSPEND:
        gc9a01_enter_sleep(dev);
        
        break;
    default:
        ret = -ENOTSUP;
        break;
    }

    return ret;
}
#endif /* CONFIG_PM_DEVICE */

static const struct display_driver_api gc9a01_api = {
    .blanking_on = gc9a01_blanking_on,
    .blanking_off = gc9a01_blanking_off,
    .write = gc9a01_write,
    .read = gc9a01_read,
    .get_framebuffer = gc9a01_get_framebuffer,
    .set_brightness = gc9a01_set_brightness,
    .set_contrast = gc9a01_set_contrast,
    .get_capabilities = gc9a01_get_capabilities,
    .set_pixel_format = gc9a01_set_pixel_format,
    .set_orientation = gc9a01_set_orientation,
};

#define gc9a01_INIT(inst)							\
    static const struct gc9a01_config gc9a01_config_##inst = {		\
        .bus = SPI_DT_SPEC_INST_GET(inst, SPI_OP_MODE_MASTER |          \
                                    SPI_WORD_SET(8), 0),		\
        .cmd_data_gpio = GPIO_DT_SPEC_INST_GET(inst, cmd_data_gpios),	\
        .reset_gpio = GPIO_DT_SPEC_INST_GET_OR(inst, reset_gpios, {}),	\
        .two_lcd_sel_gpio = GPIO_DT_SPEC_INST_GET_OR(inst, two_lcd_sel_gpios, {}),	\
		    .mdac = DT_INST_PROP(inst, mdac),				\
		    .colmod = DT_INST_PROP(inst, colmod),    \
        .width = DT_INST_PROP(inst, width),       \
        .height = DT_INST_PROP(inst, height),			\
    };									\
										\
    static struct gc9a01_data gc9a01_data_ ## inst = {			\
		.x_offset = DT_INST_PROP(inst, x_offset),			\
		.y_offset = DT_INST_PROP(inst, y_offset),			\
	};									\
                     \
    PM_DEVICE_DT_INST_DEFINE(inst, gc9a01_pm_action);			\
										\
    DEVICE_DT_INST_DEFINE(inst, &gc9a01_init, PM_DEVICE_DT_INST_GET(inst),	\
            &gc9a01_data_ ## inst, &gc9a01_config_ ## inst,			\
            POST_KERNEL, CONFIG_DISPLAY_INIT_PRIORITY,			\
            &gc9a01_api);

DT_INST_FOREACH_STATUS_OKAY(gc9a01_INIT)