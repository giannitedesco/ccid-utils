/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/
#ifndef _EMV_INTERNAL_H
#define _EMV_INTERNAL_H

#include <openssl/sha.h>
#include <openssl/rsa.h>
#include <openssl/engine.h>

#define EMV_RID_LEN 5
#define EMV_MAX_ADF_LEN 11

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
	unsigned int key_idx;
	uint8_t *iss_cert;
	size_t iss_cert_len;
	uint8_t *iss_exp;
	size_t iss_exp_len;
	uint8_t *iss_pubkey_r;
	size_t iss_pubkey_r_len;
	uint8_t *ssa_data;
	size_t ssa_data_len;
	RSA *iss_pubkey;
	uint8_t *cdol1;
	size_t cdol1_len;
	uint8_t *cdol2;
	size_t cdol2_len;
};

struct _emv {
	chipcard_t e_dev;
	xfr_t e_xfr;
	unsigned int e_num_apps;
	struct list_head e_apps;
	struct _emv_app *e_app;
#define EMV_APP_NONE 0
#define EMV_APP_VISA 1
#define EMV_APP_LINK 2
	unsigned int e_cur;
	struct _sda e_sda;
	union {
		struct {
		}a_link;
		struct {
		}a_visa;
	}e_u;
};

_private int _emv_read_record(emv_t e, uint8_t sfi, uint8_t record);
_private int _emv_select(emv_t e, uint8_t *name, size_t nlen);
_private int _emv_verify(emv_t e, uint8_t fmt, const uint8_t *p, uint8_t plen);
_private int _emv_get_data(emv_t e, uint8_t p1, uint8_t p2);
_private int _emv_get_proc_opts(emv_t e, const uint8_t *pdol, uint8_t len);
_private int _emv_generate_ac(emv_t e, uint8_t ref,
				const uint8_t *data, uint8_t len);

_private int _sda_get_issuer_key(struct _sda *s, RSA *key, size_t key_len);
_private int _sda_verify_ssa_data(struct _sda *s);

#endif /* _EMV_INTERNAL_H */
