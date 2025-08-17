#include <zephyr/usb/usb_device.h>
#include <zephyr/drivers/uart.h>
#include "csk_malloc.h"
#include <gcs_wakeup_service.h>
#include <zephyr/shell/shell.h>
#include <stdlib.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(wakeup_debug, LOG_LEVEL_INF);

#define UART_CMD_CONNECT_DATA_LEN 6
const uint8_t UART_CMD_CONNECT_DATA[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
const uint8_t UART_CMD_DISCONNECT_DATA[] = {0x55, 0x66, 0x77, 0x88, 0x99, 0x00};

const struct device *dev;
#define RING_BUF_SIZE (25600 * 3)
static uint8_t *ring_buffer;
static struct ring_buf ringbuf;
static volatile bool m_uart_connect = false;

static uint8_t uart_frame_sum(uint8_t *data, uint32_t length)
{
	uint32_t i;
	uint8_t sum = 0;
	for (i = 0; i < length; i++)
	{
		sum += *data++;
	}

	// LOG_DBG("uart_frame_sum :%d", sum);

	return sum;
}

static void uart_open(void)
{
	LOG_INF("uart open");

	// 需要先进行ring_buf的初始化，才能设置m_uart_connect的状态
	if (ring_buffer == NULL)
	{
		ring_buffer = csk_malloc(RING_BUF_SIZE);
		if (ring_buffer == NULL)
		{
			LOG_ERR("Failed to malloc ring_buffer");
			return;
		}

		ring_buf_init(&ringbuf, RING_BUF_SIZE, ring_buffer);
	}

	m_uart_connect = true;
}

static void uart_close(void)
{
	LOG_INF("uart close");
	uart_irq_tx_disable(dev);
	ring_buf_reset(&ringbuf);
	m_uart_connect = false;
}

static bool is_uart_connect(void)
{
	return m_uart_connect;
}

static void interrupt_handler(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);

	while (uart_irq_update(dev) && uart_irq_is_pending(dev))
	{
		if (uart_irq_rx_ready(dev))
		{
			int recv_len;
			uint8_t buffer[64];
			size_t len = sizeof(buffer);

			recv_len = uart_fifo_read(dev, buffer, len);
			if (recv_len < 0)
			{
				LOG_ERR("Failed to read UART FIFO");
				recv_len = 0;
			};
			LOG_INF("interrupt_handler recv_len: %d", recv_len);
			if (recv_len == UART_CMD_CONNECT_DATA_LEN)
			{
				if (0 == memcmp(buffer, UART_CMD_CONNECT_DATA, UART_CMD_CONNECT_DATA_LEN))
				{
					uart_open();
				}
				else if (0 == memcmp(buffer, UART_CMD_DISCONNECT_DATA, UART_CMD_CONNECT_DATA_LEN))
				{
					uart_close();
				}
			}
		}

		if (uart_irq_tx_ready(dev))
		{
			uint8_t buffer[64] = {0x55, 0x66};
			uint8_t sum = 0;
			int rb_len, send_len;

			rb_len = ring_buf_get(&ringbuf, &buffer[3], sizeof(buffer) - 4);
			if (!rb_len)
			{
				uart_irq_tx_disable(dev);
				continue;
			}

			sum = uart_frame_sum(&buffer[3], rb_len);
			buffer[2] = rb_len;
			buffer[rb_len + 3] = sum;

			send_len = uart_fifo_fill(dev, buffer, rb_len + 4);
			if (send_len < rb_len)
			{
				LOG_ERR("Drop %d bytes", rb_len - send_len);
			}

			LOG_DBG("ringbuf -> tty fifo %d bytes", send_len);
		}
	}
}

void usb_send_data(uint8_t *data, size_t len)
{
	if (is_uart_connect())
	{
		if (ring_buf_space_get(&ringbuf) < len)
		{
			LOG_ERR("usb_send_data ringbuf is not enough");
			return;
		}

		int rb_len = ring_buf_put(&ringbuf, data, len);
		if (rb_len < len)
		{
			LOG_ERR("Drop %u bytes", len - rb_len);
		}
		uart_irq_tx_enable(dev);
	}
	else
	{
		// 等待中断中处理完ring_buffer再释放内存
		csk_free(ring_buffer);
		ring_buffer = NULL;
	}
}

void usb_cdc_acm_init(void)
{
	uint32_t baudrate;
	int ret;

	dev = DEVICE_DT_GET_ONE(zephyr_cdc_acm_uart);
	if (!device_is_ready(dev))
	{
		LOG_ERR("CDC ACM device not ready");
		return;
	}

	ret = usb_enable(NULL);
	if (ret != 0)
	{
		LOG_ERR("Failed to enable USB");
		return;
	}

	/* They are optional, we use them to test the interrupt endpoint */
	ret = uart_line_ctrl_set(dev, UART_LINE_CTRL_DCD, 1);
	if (ret)
	{
		LOG_WRN("Failed to set DCD, ret code %d", ret);
	}

	ret = uart_line_ctrl_set(dev, UART_LINE_CTRL_DSR, 1);
	if (ret)
	{
		LOG_WRN("Failed to set DSR, ret code %d", ret);
	}

	/* Wait 100ms for the host to do all settings */
	k_msleep(100);

	ret = uart_line_ctrl_get(dev, UART_LINE_CTRL_BAUD_RATE, &baudrate);
	if (ret)
	{
		LOG_WRN("Failed to get baudrate, ret code %d", ret);
	}
	else
	{
		LOG_INF("Baudrate detected: %d", baudrate);
	}

	uart_irq_callback_set(dev, interrupt_handler);

	/* Enable rx interrupts */
	uart_irq_rx_enable(dev);
}

static void cmd_app_wakeup_get_debug_mode(const struct shell *shell, size_t argc, char **argv)
{
	LOG_INF("current mode is [%d]", wakeup_service_gcs_get_debug_mode());
	LOG_INF("mode range is 0 -- 1");
}

static void cmd_app_wakeup_set_debug_mode(const struct shell *shell, size_t argc, char **argv)
{
	int ret = 0;
	int enable = 0;
	if (argc != 2 || argv == NULL)
	{
		LOG_ERR("cmd_app_wakeup_set_debug_mode error, argc = %d, argv = %p", argc, argv);
		return;
	}

	enable = strtol(argv[1], NULL, 10);

	if (enable > 1 || enable < 0)
	{
		LOG_ERR("err: value must be 0 -- 1");
		return;
	}

	ret = wakeup_service_gcs_stop();

	ret |= wakeup_service_gcs_set_debug_mode(enable);

	ret |= wakeup_service_gcs_start();
	if (ret == 0)
	{
		LOG_INF("wakeup_debug_mode set enable [%d] successfully", enable);
	}
	else
	{
		LOG_ERR("err: wakeup_debug_mode set enable to [%d] failed ret:[%d]", enable, ret);
	}
}

static void cmd_app_wakeup_get_mic_gain(const struct shell *shell, size_t argc, char **argv)
{
	if (argc != 2 || argv == NULL)
	{
		LOG_ERR("cmd_app_wakeup_get_mic_gain error, argc = %d, argv = %p", argc, argv);
		return;
	}

	if (memcmp(argv[1], "a_gain", strlen(argv[1])) == 0)
	{
		LOG_INF("current mic a_gain is [%d]", wakeup_service_gcs_get_mic_a_gain());
	}
	else if (memcmp(argv[1], "d_gain", strlen(argv[1])) == 0)
	{
		LOG_INF("current mic gain is [%d]", wakeup_service_gcs_get_mic_d_gain());
	}
	else
	{
		LOG_ERR("err: wakeup_mic_gain get failed arg[1]: %s", argv[1]);
	}
}

static void cmd_app_wakeup_set_mic_gain(const struct shell *shell, size_t argc, char **argv)
{
	int ret = 0;
	int gain = 0;

	if (argc != 3 || argv == NULL)
	{
		LOG_ERR("cmd_app_wakeup_set_mic_gain error, argc = %d, argv = %p", argc, argv);
		return;
	}

	gain = strtol(argv[2], NULL, 10);

	if (memcmp(argv[1], "a_gain", strlen(argv[1])) == 0)
	{
		if (gain > 36 || gain < -12)
		{
			LOG_ERR("err: value must be [-12, 36]");
			return;
		}

		ret = wakeup_service_gcs_set_mic_a_gain(gain);
		if (ret == 0)
		{
			LOG_INF("wakeup_mic_a_gain set value [%d] successfully", gain);
		}
		else
		{
			LOG_ERR("err: wakeup_mic_a_gain set to [%d] failed ret:[%d]", gain, ret);
		}
	}
	else if (memcmp(argv[1], "d_gain", strlen(argv[1])) == 0)
	{
		if (gain > 42 || gain < -83)
		{
			LOG_ERR("err: value must be [-83, 42]");
			return;
		}

		ret = wakeup_service_gcs_set_mic_d_gain(gain);
		if (ret == 0)
		{
			LOG_INF("wakeup_mic_d_gain set value [%d] successfully", gain);
		}
		else
		{
			LOG_ERR("err: wakeup_mic_d_gain set to [%d] failed ret:[%d]", gain, ret);
		}
	}
	else
	{
		LOG_ERR("err: wakeup_mic_gain set failed arg[1]: %s", argv[1]);
	}
}

static void cmd_app_wakeup_get_ref_gain(const struct shell *shell, size_t argc, char **argv)
{
	LOG_INF("current ref gain is [%d]", wakeup_service_gcs_get_ref_gain());
}

static void cmd_app_wakeup_set_ref_gain(const struct shell *shell, size_t argc, char **argv)
{
	int ret = 0;
	int gain = 0;
	if (argc != 2 || argv == NULL)
	{
		LOG_ERR("cmd_app_wakeup_set_ref_gain error, argc = %d, argv = %p", argc, argv);
		return;
	}

	gain = strtol(argv[1], NULL, 10);

	if (gain > 200 || gain < -200)
	{
		LOG_ERR("err: value must be 0 -- 500");
		return;
	}

	ret = wakeup_service_gcs_set_ref_gain(gain);
	if (ret == 0)
	{
		LOG_INF("wakeup_ref_gain set value [%d] successfully", gain);
	}
	else
	{
		LOG_ERR("err: wakeup_ref_gain set enable to [%d] failed ret:[%d]", gain, ret);
	}
}

#define MODE_GET_TEXT "<mode> <get>"
#define MODE_SET_TEXT "<mode> <set> <value>"
#define MIC_GET_TEXT "<mic> <get> <a_or_d_gain>"
#define MIC_SET_TEXT "<mic> <set> <a_or_d_gain> <value>"
#define REF_GET_TEXT "<ref> <get> <value>"
#define REF_SET_TEXT "<ref> <set> <value>"

SHELL_STATIC_SUBCMD_SET_CREATE(sub_debug_mode,
							   SHELL_CMD_ARG(get, NULL, MODE_GET_TEXT, cmd_app_wakeup_get_debug_mode, 1, 0),
							   SHELL_CMD_ARG(set, NULL, MODE_SET_TEXT, cmd_app_wakeup_set_debug_mode, 2, 0),
							   SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_mic_gain,
							   SHELL_CMD_ARG(get, NULL, MIC_GET_TEXT, cmd_app_wakeup_get_mic_gain, 2, 0),
							   SHELL_CMD_ARG(set, NULL, MIC_SET_TEXT, cmd_app_wakeup_set_mic_gain, 3, 0),
							   SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_ref_gain,
							   SHELL_CMD_ARG(get, NULL, REF_GET_TEXT, cmd_app_wakeup_get_ref_gain, 1, 0),
							   SHELL_CMD_ARG(set, NULL, REF_SET_TEXT, cmd_app_wakeup_set_ref_gain, 2, 0),
							   SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(mode, &sub_debug_mode, "wakeup debug mode related commands.", NULL);
SHELL_CMD_REGISTER(mic, &sub_mic_gain, "wakeup gain info related commands.", NULL);
SHELL_CMD_REGISTER(ref, &sub_ref_gain, "wakeup gain info related commands.", NULL);
