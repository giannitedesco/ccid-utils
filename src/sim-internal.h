/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/
#ifndef _SIM_INTERNAL_H
#define _SIM_INTERNAL_H

#define SIM_CLA			0xa0

#define SIM_INS_SELECT		0xa4
#define SIM_INS_READ_RECORD	0xb2
#define SIM_INS_GET_RESPONSE	0xc0

#define SIM_MF			0x3f00
#define SIM_DF_TELECOM		0x7f10
#define SIM_DF_GSM		0x7f20
#define SIM_EF_ICCID		0x2fe2
#define SIM_EF_SMS		0x6f3c

#define SIM_SW1_SUCCESS		0x90
#define SIM_SW1_SHORT		0x9f
#define SIM_SW1_MEM_ERR		0x92
#define SIM_SW1_REF_ERR		0x94
#define SIM_SW1_SEC_ERR		0x98

#define SIM_TYPE_MASK		0xff00
#define SIM_MF			0x3f00
#define SIM_TYPE_DF		0x7f00
#define SIM_TYPE_EF		0x6f00
#define SIM_TYPE_ROOT_EF	0x2f00
#define SIM_FILE_INVALID	0xffff
#define SIM_IS_MF(x)		((x & SIM_TYPE_MASK) == SIM_MF)
#define SIM_IS_DF(x)		((x & SIM_TYPE_MASK) == SIM_TYPE_DF)
#define SIM_IS_EF(x)		((x & SIM_TYPE_MASK) == SIM_TYPE_EF)
#define SIM_IS_RoOT_EF(x)	((x & SIM_TYPE_MASK) == SIM_TYPE_ROOT_EF)

struct df_fci {
	uint16_t f_rfu0;
	uint16_t f_free;
	uint16_t f_id;
	uint8_t  f_type;
	uint8_t  f_rfu1[5];
	uint8_t  f_opt_len;
} _packed;

struct df_gsm {
	uint8_t g_fch;
	uint8_t g_num_df;
	uint8_t g_num_ef;
	uint8_t g_chv;
	uint8_t g_rfu0;
	uint8_t g_chv1;
	uint8_t g_chv1u;
	uint8_t g_chv2;
	uint8_t g_chv2u;
} _packed;

struct ef_fci {
	uint16_t e_rfu;
	uint16_t e_size;
	uint16_t e_id;
	uint8_t  e_type;
	uint8_t  e_increase;
	uint8_t  e_access[3];
	uint8_t  e_status;
	uint8_t  e_opt_len;
	uint8_t  e_struct;
	uint8_t  e_reclen;
} _packed;

struct _sim {
	chipcard_t	s_cc;
	xfr_t		s_xfr;
	uint16_t	s_df;
	uint16_t	s_ef;
	uint8_t		s_reclen;
	struct df_fci	s_df_fci;
	struct df_gsm 	s_df_gsm;
	struct ef_fci	s_ef_fci;
};

_private int _sim_select(struct _sim *s, uint16_t id);

#endif /* _SIM_INTERNAL_H */
