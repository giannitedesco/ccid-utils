/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/
#ifndef _EMV_H
#define _EMV_H

#define EMV_AIP_LEN 		2
#define EMV_RID_LEN		5
#define EMV_AID_LEN		11

#define EMV_DATA_BINARY		0x0
#define EMV_DATA_TEXT		0x1
#define EMV_DATA_INT		0x2
#define EMV_DATA_BCD		0x3
#define EMV_DATA_DATE		0x4

#define EMV_ERR_SYSTEM			0x0
#define EMV_ERR_CCID			0x1
#define EMV_ERR_ICC			0x2
#define EMV_ERR_EMV			0x3
#define  EMV_ERR_SUCCESS		0x00
#define  EMV_ERR_DATA_ELEMENT_NOT_FOUND 0x01
#define  EMV_ERR_BAD_PIN_FORMAT		0x02
#define  EMV_ERR_FUNC_NOT_SUPPORTED	0x03
#define  EMV_ERR_KEY_NOT_FOUND		0x04
#define  EMV_ERR_KEY_SIZE_MISMATCH	0x05
#define  EMV_ERR_RSA_RECOVERY		0x06
#define  EMV_ERR_CERTIFICATE		0x07
#define  EMV_ERR_SSA_SIGNATURE		0x08
#define  EMV_ERR_BAD_PIN		0x09
#define  EMV_ERR_BER_DECODE		0x0a
#define  EMV_ERR_APP_NOT_SELECTED	0x0b
typedef uint32_t emv_err_t;

typedef struct _emv *emv_t;
typedef struct _emv_app *emv_app_t;
typedef uint8_t emv_aip_t[EMV_AIP_LEN];
typedef const struct _emv_data *emv_data_t;
typedef uint8_t emv_rid_t[EMV_RID_LEN];
typedef const uint8_t *(*emv_mod_cb_t)(void *priv, unsigned int index,
					size_t *len);
typedef const uint8_t *(*emv_exp_cb_t)(void *priv, unsigned int index,
					size_t *len);
typedef int (*emv_dol_cb_t)(uint16_t tag, uint8_t *ptr, size_t len, void *priv);

/* Setup/teardown */
_public emv_t emv_init(cci_t cc);
_public void emv_fini(emv_t e);

/* error handling */
_public emv_err_t emv_error(emv_t e);
_public unsigned int emv_error_type(emv_err_t e);
_public unsigned int emv_error_additional(emv_err_t e);
_public const char *emv_error_string(emv_err_t err);

/* Application selection */
_public emv_app_t emv_current_app(emv_t e);
_public int emv_appsel_pse(emv_t e);
_public emv_app_t emv_appsel_pse_first(emv_t e);
_public emv_app_t emv_appsel_pse_next(emv_t e, emv_app_t app);
_public int emv_app_select_pse(emv_t e, emv_app_t app);
_public int emv_app_select_aid(emv_t e, const uint8_t *aid, size_t len);
_public int emv_app_select_aid_next(emv_t e, const uint8_t *aid, size_t len);

/* EMV applications */
_public void emv_app_delete(emv_app_t a);
_public void emv_app_rid(emv_app_t a, emv_rid_t ret);
_public void emv_app_aid(emv_app_t a, uint8_t *ret, size_t *len);
_public const char *emv_app_label(emv_app_t a);
_public const char *emv_app_pname(emv_app_t a);
_public uint8_t emv_app_prio(emv_app_t a);
_public int emv_app_confirm(emv_app_t a);

/* Application initiation */
_public int emv_app_init(emv_t e);
_public int emv_app_aip(emv_t e, emv_aip_t aip);

/* Application data */
_public int emv_read_app_data(emv_t e);
_public emv_data_t emv_retrieve_data(emv_t e, uint16_t id);
_public emv_data_t *emv_retrieve_records(emv_t e, unsigned int *nmemb);

_public emv_data_t *emv_data_children(emv_data_t d, unsigned int *nmemb);
_public const uint8_t *emv_data(emv_data_t d, size_t *len);
_public int emv_data_int(emv_data_t d);
_public int emv_data_sda(emv_data_t d);
_public unsigned int emv_data_type(emv_data_t d);
_public uint16_t emv_data_tag(emv_data_t d);
_public const char *emv_data_tag_label(emv_data_t d);

/* Static data authentication */
_public int emv_authenticate_static_data(emv_t e, emv_mod_cb_t mod,
						emv_exp_cb_t exp, void *priv);
_public int emv_sda_ok(emv_t e);

/* Dynamic data authentication */
_public int emv_authenticate_dynamic(emv_t e, emv_mod_cb_t mod,
					emv_exp_cb_t exp, void *priv);
_public int emv_dda_ok(emv_t e);

/* Cardholder verification, only offline plaintext pin supported for now */
_public int emv_cvm_pin(emv_t e, const char *pin);
_public int emv_pin_try_counter(emv_t e);

/* Terminal risk management */
_public int emv_trm_last_online_atc(emv_t e);
_public int emv_trm_atc(emv_t e);

/* Utility function for constructing DOL's */
_public uint8_t *emv_construct_dol(emv_dol_cb_t cbfn,
					const uint8_t *ptr, size_t len,
					size_t *ret_len, void *priv);

_public const uint8_t *emv_generate_ac(emv_t e, uint8_t ref,
					const uint8_t *tx, uint8_t len,
					size_t *rlen);

/* Definitions from EMV spec */
#define EMV_AC_AAC		0x00 /* app auth cryptogram . decliend */
#define EMV_AC_TC		0x40 /* transaction certificate / approved */
#define EMV_AC_ARQC		0x80 /* auth request / online requested */
#define EMV_AC_CDA		0x10 /* cda signature requested */

/* Application Interchange Profile */
#define EMV_AIP_CDA		0x01 /* CDA support */
#define EMV_AIP_ISS		0x04 /* issuer authentication support */
#define EMV_AIP_TRM		0x08 /* terminal risk management required */
#define EMV_AIP_CVM		0x10 /* cardholder verification support */
#define EMV_AIP_DDA		0x20 /* DDA support */
#define EMV_AIP_SDA		0x40 /* SDA support */

/* Application Usage Control */
#define EMV_AUC1_DOMESTIC_CASH		(1<<7)
#define EMV_AUC1_INT_CASH		(1<<6)
#define EMV_AUC1_DOMESTIC_GOODS		(1<<5)
#define EMV_AUC1_INT_GOODS		(1<<4)
#define EMV_AUC1_DOMESTIC_SERVICES	(1<<3)
#define EMV_AUC1_INT_SERVICES		(1<<2)
#define EMV_AUC1_ATM			(1<<1)
#define EMV_AUC1_NON_ATM_TERMINALS	(1<<0)

#define EMV_AUC2_DOMESTIC_CASHBACK	(1<<7)
#define EMV_AUC2_INT_CASHBACK		(1<<6)

/* Selected data items (seen on EMV cards issued in UK) */
#define EMV_TAG_MAGSTRIP_TRACK2		0x0057
#define EMV_TAG_PAN			0x005a
#define EMV_TAG_RECORD			0x0070
#define EMV_TAG_CDOL1			0x008c
#define EMV_TAG_CDOL2			0x008d
#define EMV_TAG_CVM_LIST		0x008e
#define EMV_TAG_CA_PK_INDEX		0x008f
#define EMV_TAG_ISS_PK_CERT		0x0090
#define EMV_TAG_ISS_PK_R		0x0092
#define EMV_TAG_SSA_DATA		0x0093
#define EMV_TAG_CARDHOLDER_NAME		0x5f20
#define EMV_TAG_DATE_EXP		0x5f24
#define EMV_TAG_DATE_EFF		0x5f25
#define EMV_TAG_ISSUER_COUNTRY		0x5f28
#define EMV_TAG_SERVICE_CODE		0x5f30
#define EMV_TAG_PAN_SEQ			0x5f34
#define EMV_TAG_USAGE_CONTROL		0x9f07
#define EMV_TAG_APP_VER			0x9f08
#define EMV_TAG_IAC_DEFAULT		0x9f0d
#define EMV_TAG_IAC_DENY		0x9f0e
#define EMV_TAG_IAC_ONLINE		0x9f0f
#define EMV_TAG_MAGSTRIP_TRACK1		0x9f1f
#define EMV_TAG_ISS_PK_EXP		0x9f32
#define EMV_TAG_CURRENCY_EXP		0x9f44
#define EMV_TAG_ICC_PK_CERT		0x9f46
#define EMV_TAG_ICC_PK_EXP		0x9f47
#define EMV_TAG_ICC_PK_R		0x9f48
#define EMV_TAG_DDOL			0x9f49
#define EMV_TAG_SDA_TAG_LIST		0x9f4a

#define EMV_TAG_UNPREDICTABLE_NUMBER	0x9f37

#endif /* _EMV_H */
