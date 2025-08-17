#include <zephyr/kernel.h>

#include <comp_res_gcl.h>
#include <csk_malloc.h>
#include <zephyr/devicetree.h>
#define COMPONENTS_RESOURCE_ITEMS_MAX (16)


#define DT_RES_ADDR(name) DT_REG_ADDR(DT_CHOSEN(name))
#define DT_RES_SIZE(name) DT_REG_SIZE(DT_CHOSEN(name))

int components_resource_load(void)
{
	gcl_comp_res_mgr_ipc_t *res_mgr;
	uint32_t res_mgr_size;

	res_mgr_size = sizeof(gcl_comp_res_mgr_ipc_t) + sizeof(gcl_comp_res_mgr_item_ipc_t) * COMPONENTS_RESOURCE_ITEMS_MAX;
	res_mgr = csk_malloc(res_mgr_size);
	res_mgr->number = 0;

#if CONFIG_CAPABILITY_SPD
	res_mgr->item[res_mgr->number].res_type = COMP_RES_GCL_IPC_SPD_BODY_DETECT;
	res_mgr->item[res_mgr->number].res_addr = DT_RES_ADDR(resource_spd_body_detect);
	res_mgr->item[res_mgr->number].res_size = DT_RES_SIZE(resource_spd_body_detect);
	res_mgr->number++;

	res_mgr->item[res_mgr->number].res_type = COMP_RES_GCL_IPC_SPD_KEY_POINTS;
	res_mgr->item[res_mgr->number].res_addr = DT_RES_ADDR(resource_spd_key_points);
	res_mgr->item[res_mgr->number].res_size = DT_RES_SIZE(resource_spd_key_points);
	res_mgr->number++;

	res_mgr->item[res_mgr->number].res_type = COMP_RES_GCL_IPC_SPD_SIT_POSE;
	res_mgr->item[res_mgr->number].res_addr = DT_RES_ADDR(resource_spd_sit_pose);
	res_mgr->item[res_mgr->number].res_size = DT_RES_SIZE(resource_spd_sit_pose);
	res_mgr->number++;
#endif

#if CONFIG_CAPABILITY_WAKEUP
	res_mgr->item[res_mgr->number].res_type = COMP_RES_GCL_IPC_WAKEUP_CAE_MLP;
	res_mgr->item[res_mgr->number].res_addr = DT_RES_ADDR(resource_wakeup_cae_mlp);
	res_mgr->item[res_mgr->number].res_size = DT_RES_SIZE(resource_wakeup_cae_mlp);
	res_mgr->number++;

	res_mgr->item[res_mgr->number].res_type = COMP_RES_GCL_IPC_WAKEUP_ESR_MLP;
	res_mgr->item[res_mgr->number].res_addr = DT_RES_ADDR(resource_wakeup_esr_mlp);
	res_mgr->item[res_mgr->number].res_size = DT_RES_SIZE(resource_wakeup_esr_mlp);
	res_mgr->number++;

	res_mgr->item[res_mgr->number].res_type = COMP_RES_GCL_IPC_WAKEUP_ESR_MAIN;
	res_mgr->item[res_mgr->number].res_addr = DT_RES_ADDR(resource_wakeup_esr_main);
	res_mgr->item[res_mgr->number].res_size = DT_RES_SIZE(resource_wakeup_esr_main);
	res_mgr->number++;

	res_mgr->item[res_mgr->number].res_type = COMP_RES_GCL_IPC_WAKEUP_ESR_CMDS;
	res_mgr->item[res_mgr->number].res_addr = DT_RES_ADDR(resource_wakeup_esr_cmds);
	res_mgr->item[res_mgr->number].res_size = DT_RES_SIZE(resource_wakeup_esr_cmds);
	res_mgr->number++;

	res_mgr->item[res_mgr->number].res_type = COMP_RES_GCL_IPC_WAKEUP_AI_WRAP_CONFIG;
	res_mgr->item[res_mgr->number].res_addr = DT_RES_ADDR(resource_wakeup_ai_wrap_conf);
	res_mgr->item[res_mgr->number].res_size = DT_RES_SIZE(resource_wakeup_ai_wrap_conf);
	res_mgr->number++;

#endif

#if CONFIG_CAPABILITY_QRCODE
	res_mgr->item[res_mgr->number].res_type = COMP_RES_GCL_IPC_QRCODE_DETECT;
	res_mgr->item[res_mgr->number].res_addr = DT_RES_ADDR(resource_qrcode_detect);
	res_mgr->item[res_mgr->number].res_size = DT_RES_SIZE(resource_qrcode_detect);
	res_mgr->number++;
#endif

	gcl_comps_res_load(res_mgr);
	csk_free(res_mgr);

    return 0;
}