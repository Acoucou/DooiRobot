#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/storage/flash_map.h>
#include "boot_cp.h"
#include "ic_message.h"
#include <stdint.h>
#include <stddef.h>
#include "resource.h"
#include "app_player.h"
#include "app_connect.h"
#include "app_chat.h"
#include "app_chat_wakeup.h"
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);


#define CP_FLASH_BASE_ADDRESS (0x68000000)
#define FLASH_CP_IMAGE_REGION_OFFSET DT_REG_ADDR(DT_NODE_BY_FIXED_PARTITION_LABEL(cp_code))
#define CP_BOOT_ADDRESS (CP_FLASH_BASE_ADDRESS + FLASH_CP_IMAGE_REGION_OFFSET)

#define MAX_PLAYER_NUMS 3
player_focus_action_e ops_table[MAX_PLAYER_NUMS][MAX_PLAYER_NUMS] = {
		/*                   TONE_PLAYER                   TTS_PLAYER          MUSIC_PLAYER*/
		/*TONE_PLAYER*/ {PLAYER_FOCUS_CANCELED, PLAYER_FOCUS_CANCELED, PLAYER_FOCUS_PAUSED},
		/*TTS_PLAYER*/ {PLAYER_FOCUS_BACKOFF, PLAYER_FOCUS_CANCELED, PLAYER_FOCUS_PAUSED},
		/*MUSIC_PLAYER*/ {PLAYER_FOCUS_BACKOFF, PLAYER_FOCUS_QUEUED, PLAYER_FOCUS_CANCELED},
};

int main(void)
{
	boot_cp((const void *)CP_BOOT_ADDRESS);
#if (CONFIG_WAKEUP_DEBUG)
	extern void usb_cdc_acm_init(void);
	usb_cdc_acm_init();
#endif
	ic_message_init();
#if (CONFIG_GCL_COMP_RES)
	components_resource_load();
#endif

	srand((unsigned)time(NULL));
	
	extern void ble_connect_task_init();
	ble_connect_task_init();

	extern int camera_init(void);
	camera_init();

	app_player_config_t config = {
			.focus_config =
					{
							.focus_table = (player_focus_action_e *)ops_table,
							.row_column_num = MAX_PLAYER_NUMS,
					},
			.app_player_num = MAX_PLAYER_NUMS,
	};
	app_player_init(&config);

	app_chat_wakeup_init();

	app_connect_init();

	app_chat_init();
}
