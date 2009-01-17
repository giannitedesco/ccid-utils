/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/
#ifndef _SIM_H
#define _SIM_H

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
#define SIM_TYPE_MF		0x3f00
#define SIM_TYPE_DF		0x7f00
#define SIM_TYPE_EF		0x6f00
#define SIM_TYPE_ROOT_EF	0x2f00
#define SIM_IS_MF(x)		((x & SIM_TYPE_MASK) == SIM_TYPE_MF)
#define SIM_IS_DF(x)		((x & SIM_TYPE_MASK) == SIM_TYPE_DF)
#define SIM_IS_EF(x)		((x & SIM_TYPE_MASK) == SIM_TYPE_EF)
#define SIM_IS_RoOT_EF(x)	((x & SIM_TYPE_MASK) == SIM_TYPE_ROOT_EF)

typedef struct _sim *sim_t;

sim_t sim_new(chipcard_t cc);
void sim_free(sim_t sim);
int sim_select(sim_t sim, uint16_t id);
const uint8_t *sim_read_record(sim_t sim, uint8_t rec, size_t *rec_len);

#endif /* _GSM_H */
