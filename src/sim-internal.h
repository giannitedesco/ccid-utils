/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/
#ifndef _SIM_INTERNAL_H
#define _SIM_INTERNAL_H

#define SIM_CLA			0xa0

#define SIM_INS_SELECT		0xa4
#define SIM_INS_READ_BINARY	0xb0
#define SIM_INS_READ_RECORD	0xb2
#define SIM_INS_GET_RESPONSE	0xc0

#define SIM_MF			0x3f00
#define SIM_EF_ICCID		0x2fe2

#define SIM_DF_TELECOM		0x7f10

#define SIM_DF_GSM		0x7f20
#define SIM_EF_SMS		0x6f3c
#define SIM_EF_IMSI		0x6f07

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

#define EF_TRANSPARENT		0x0
#define EF_LINEAR		0x1
#define EF_CYCLIC		0x3

/* SMS record status codes */
#define SIM_SMS_STATUS_FREE	0
#define SIM_SMS_STATUS_READ	1
#define SIM_SMS_STATUS_UNREAD	3
#define SIM_SMS_STATUS_SENT	5
#define SIM_SMS_STATUS_UNSENT	7

/* SMS-DELIVER octet codes */
#define SMS_TP_MTI		((1<<0)|(1<<1)) /* message type indicator */
#define SMS_TP_MMS		(1<<2) /* more messages */
#define SMS_TP_SRI		(1<<5) /* status report indicator */
#define SMS_TP_UDHI		(1<<6) /* user data header included */
#define SMS_TP_RP		(1<<7) /* return path included */

/* Type of phone number */
#define GSM_NUMBER_TYPE_MASK	(7 << 4)
#define GSM_NUMBER_UNKNOWN	(0 << 4)
#define GSM_NUMBER_INTL		(1 << 4)
#define GSM_NUMBER_NATIONAL	(2 << 4)
#define GSM_NUMBER_NET_SPEC	(3 << 4)
#define GSM_NUMBER_SUBSCR	(4 << 4)
#define GSM_NUMBER_ABBREV	(5 << 4)
#define GSM_NUMBER_ALNUM	(6 << 4)
#define GSM_NUMBER_RESERVED	(7 << 4)

/* Phone numbering plan */
#define GSM_PLAN_MASK		0xf
#define GSM_PLAN_UNKNOWN	0
#define GSM_PLAN_ISDN		1
#define GSM_PLAN_X121		3
#define GSM_PLAN_TELEX		4
#define GSM_PLAN_NATIONAL	8
#define GSM_PLAN_PRIVATE	9
#define GSM_PLAN_ERMES		10

struct fci {
	uint16_t f_rfu;
	uint16_t f_size;
	uint16_t f_id;
	uint8_t  f_type;
} _packed;

struct df_fci {
	uint16_t d_rfu0;
	uint16_t d_free;
	uint16_t d_id;
	uint8_t  d_type;
	uint8_t  d_rfu1[5];
	uint8_t  d_opt_len;
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

#define EF_FCI_MIN_OPT_LEN 2
struct ef_fci {
	uint16_t e_rfu;
	uint16_t e_size;
	uint16_t e_id;
	uint8_t  e_type;
	uint8_t  e_increase;
	uint8_t  e_access[3];
	uint8_t  e_status;
	uint8_t  e_opt_len;
	uint8_t	 e_struct;
	uint8_t	 e_reclen;
} _packed;

struct _sms {
	const uint8_t *smsc;
	const uint8_t *sender;
	const uint8_t *data;
	uint8_t smsc_len;
	uint8_t smsc_type;
	uint8_t sender_len;
	uint8_t sender_type;
	uint8_t tp_pid;
	uint8_t tp_dcs;
	uint8_t uda;
	uint8_t status;
	uint8_t sms_deliver;
	uint8_t timestamp[7];
};

struct _sim {
	cci_t	s_cc;
	xfr_t		s_xfr;
	uint16_t	s_df;
	uint16_t	s_ef;
	uint8_t		s_reclen;
	struct df_fci	s_df_fci;
	struct df_gsm 	s_df_gsm;
	struct ef_fci	s_ef_fci;
};

_private int _apdu_select(struct _sim *s, uint16_t id);
_private int _apdu_read_binary(struct _sim *s, uint16_t ofs, uint8_t len);
_private int _apdu_read_record(struct _sim *s, uint8_t rec, uint8_t len);
_private void _sms_decode(struct _sms *, const uint8_t *ptr); /* 175 bytes */

#endif /* _SIM_INTERNAL_H */
