/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/
#ifndef _EMV_INTERNAL_H
#define _EMV_INTERNAL_H

#define EMV_RID_LEN 5
#define EMV_MAX_ADF_LEN 11
#define EMV_AIP_LEN 2

#define EMV_PIN_BLOCK_LEN 8

#define EMV_AIP_CDA	0x01 /* CDA support */
#define EMV_AIP_ISS	0x04 /* issuer authentication support */
#define EMV_AIP_TRM	0x08 /* terminal risk management required */
#define EMV_AIP_CVM	0x10 /* cardholder verification support */
#define EMV_AIP_DDA	0x20 /* DDA support */
#define EMV_AIP_SDA	0x40 /* SDA support */

#include <openssl/sha.h>
#include <openssl/rsa.h>
#include <openssl/engine.h>

typedef uint8_t emv_pb_t[EMV_PIN_BLOCK_LEN];

struct _emv_app {
	uint8_t a_recno;
	uint8_t a_prio;
	uint8_t a_id_sz;
	/* uint8_t a_pad0; */
	uint8_t a_id[16];
	char a_name[16];
	char a_pname[16];
	struct list_head a_list;
};

struct _sda {
	/* CA public key index */
	unsigned int key_idx;

	/* Issuer certificate */
	uint8_t *iss_cert;
	size_t iss_cert_len;

	/* issuer public key exponent */
	uint8_t *iss_exp;
	size_t iss_exp_len;

	/* issuer public key remainder */
	uint8_t *iss_pubkey_r;
	size_t iss_pubkey_r_len;

	/* Signed ssa data */
	uint8_t *ssa_data;
	size_t ssa_data_len;

	uint8_t *data;
	size_t data_len;

	RSA *iss_pubkey;
};

struct _emv {
	/* hardware */
	chipcard_t e_dev;
	xfr_t e_xfr;

	/* application selection */
	unsigned int e_num_apps;
	struct list_head e_apps;
	struct _emv_app *e_app;
#define EMV_APP_NONE 0
#define EMV_APP_VISA 1
#define EMV_APP_LINK 2
	unsigned int e_cur;

	/* ICC data */
	uint8_t e_aip[EMV_AIP_LEN];
	uint8_t *e_afl;
	size_t e_afl_len;
	uint8_t *e_cdol1;
	size_t e_cdol1_len;
	uint8_t *e_cdol2;
	size_t e_cdol2_len;

	/* Data for SDA */
	struct _sda e_sda;

	/* Any app specific data */
	union {
		struct {
		}a_link;
		struct {
		}a_visa;
	}e_u;
};

/* Utility functions */
_private int _emv_pin2pb(const char *pin, uint8_t *pb);
_private uint8_t *_emv_construct_dol(struct ber_tag *tags,
					size_t num_tags,
					const uint8_t *ptr, size_t len,
					size_t *ret_len, void *priv);

/* Application selection */
_private void _emv_free_applist(emv_t e);
_private void _emv_init_applist(emv_t e);

/* Application transaction initiation */
_private int _emv_app_init(emv_t e, const uint8_t *aid, size_t aid_len);

/* Application data retrieval */
_private int _emv_read_app_data(struct _emv *e);
_private int _emv_read_sda_data(struct _emv *e);
_private int _emv_pin_try_counter(struct _emv *e);

/* APDU construction + transactions */
_private int _emv_read_record(emv_t e, uint8_t sfi, uint8_t record);
_private int _emv_select(emv_t e, const uint8_t *name, size_t nlen);
_private int _emv_select_next(emv_t e, const uint8_t *name, size_t nlen);
_private int _emv_verify(emv_t e, uint8_t fmt, const uint8_t *p, uint8_t plen);
_private int _emv_get_data(emv_t e, uint8_t p1, uint8_t p2);
_private int _emv_get_proc_opts(emv_t e, const uint8_t *pdol, uint8_t len);
_private int _emv_generate_ac(emv_t e, uint8_t ref,
				const uint8_t *data, uint8_t len);

/* Static data authentication */
_private int _sda_get_issuer_key(struct _sda *s, RSA *key, size_t key_len);
_private int _sda_verify_ssa_data(struct _sda *s);

#endif /* _EMV_INTERNAL_H */
