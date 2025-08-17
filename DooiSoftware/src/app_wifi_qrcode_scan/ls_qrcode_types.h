#ifndef _LS_QRCODE_TYPES_H_
#define _LS_QRCODE_TYPES_H_

#include <stdint.h>
#include "ls_qrcode_defines.h"

typedef void *LS_QRCODE_INST;

typedef enum _ls_qrcode_res_type
{
	RES_QRCODE_DETECT  = 1,
	RES_QRCODE_COUNT   = 2,
	RES_BODY_MAX = 0XFFFFFFFF,
}ls_qrcode_res_type;

typedef struct _qrcode_res
{
	ls_qrcode_res_type  res_buffer_type;
	void*               res_buffer_addr;
	unsigned int        res_buffer_size;
}qrcode_res;

typedef struct _qrcode_res_mgr
{
	qrcode_res res_type_arr_[MAX_QRCODE_RES_TYPE];
}qrcode_res_mgr, *p_qrcode_res_mgr;

typedef enum _ls_pixel_format
{
	LS_PIX_FMT_GRAY8  = 1,
	LS_PIX_FMT_BGR888 = 3,
	LS_PIX_FMT_MAX    = 0XFFFFFFFF,
}ls_pixel_format;

typedef struct _ls_qrcode_inst_param
{
	void *ram_addr;
	void *psram_addr;

	unsigned int ram_total;
	unsigned int ram_used;
	unsigned int psram_total;
	unsigned int psram_used;
}ls_qrcode_inst_param, *p_ls_qrcode_inst_param;

typedef struct _ls_rect
{
	int   x;
	int   y;
	int   w;
	int   h;
}ls_rect;

typedef struct _ls_point
{
	float x;
	float y;
	float v_score;
}ls_point;

typedef struct _qr_code
{
	int			  version;
	int			  ecc_level;
	int			  mask;
	int			  data_type;
	unsigned char payload[QRCODE_MAX_PAYLOAD];
	int			  payload_len;
	unsigned int  eci;
}qr_code;

typedef struct _ls_qrcode_detect
{
	ls_rect	 qrcode_rect;
	float    qrcode_score;
	qr_code  qrcode_result;
}ls_qrcode_detect;

#endif 