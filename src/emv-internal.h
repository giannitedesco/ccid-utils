/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/
#ifndef _EMV_INTERNAL_H
#define _EMV_INTERNAL_H

#define EMV_AIP_LEN 2

#define EMV_PIN_BLOCK_LEN 8

#define EMV_AIP_CDA	0x01 /* CDA support */
#define EMV_AIP_ISS	0x04 /* issuer authentication support */
#define EMV_AIP_TRM	0x08 /* terminal risk management required */
#define EMV_AIP_CVM	0x10 /* cardholder verification support */
#define EMV_AIP_DDA	0x20 /* DDA support */
#define EMV_AIP_SDA	0x40 /* SDA support */

#define EMV_AC_AAC	0x00 /* app authnticate  cryptogram . decliend */
#define EMV_AC_TC	0x40 /* transaction certificate / approved */
#define EMV_AC_ARQC	0x80 /* authorisation request / online requested */
#define EMV_AC_CDA	0x10 /* cda signature requested */

#include <openssl/sha.h>
#include <openssl/rsa.h>
#include <openssl/engine.h>

#include <gang.h>
#include <mpool.h>

#define EMV_ERR_TYPE_SHIFT	30
#define EMV_ERR_CODE_MASK	((1 << EMV_ERR_TYPE_SHIFT) - 1)

typedef uint8_t emv_pb_t[EMV_PIN_BLOCK_LEN];

#define EMV_DATA_SDA		(1<<0)

#define EMV_DATA_ATOMIC		(1<<15)
#define EMV_DATA_DOL		(1<<14)
#define EMV_DATA_TYPE_MASK 	((1<<14)-1)
struct _emv_tag {
	uint16_t t_tag;
	uint16_t t_type;
	uint8_t t_min, t_max;
	const char *t_label;
};

struct _emv_data {
	const struct _emv_tag *d_tag;
	uint16_t d_id;
	uint16_t d_flags;
	const uint8_t *d_data;
	size_t d_len;
	struct _emv_data **d_elem;
	unsigned int d_nmemb;
};

static inline int emv_data_atomic(struct _emv_data *d)
{
	return !!(d->d_tag->t_type & EMV_DATA_ATOMIC);
}
static inline int emv_data_composite(struct _emv_data *d)
{
	return !(d->d_tag->t_type & EMV_DATA_ATOMIC);
}

struct _emv_db {
	unsigned int db_nmemb;
	struct _emv_data **db_elem;
	unsigned int db_numrec;
	struct _emv_data **db_rec;
	unsigned int db_numsda;
	struct _emv_data **db_sda;
};

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

struct _emv {
	/* hardware */
	chipcard_t e_dev;
	xfr_t e_xfr;

	mpool_t e_data;
	gang_t e_files;
	struct _emv_db e_db;

	/* application selection */
	unsigned int e_num_apps;
	struct list_head e_apps;
	struct _emv_app *e_app;

	uint8_t e_aip[2];
	uint8_t *e_afl;
	size_t e_afl_len;

	/* crypto stuff */
	int e_sda_ok;
	RSA *e_ca_pk;
	RSA *e_iss_pk;

	emv_err_t e_err;
};

#define DOL_NUM_TAGS(x) (sizeof(x)/sizeof(struct dol_tag))
struct dol_tag {
	const char *tag;
	size_t tag_len;
	int(*op)(uint8_t *ptr, size_t len, void *priv);
};

/* Utility functions */
_private uint8_t _emv_sw1(emv_t e);
_private uint8_t _emv_sw2(emv_t e);
_private int _emv_pin2pb(const char *pin, uint8_t *pb);
_private uint8_t *_emv_construct_dol(const struct dol_tag *tags,
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
_private const struct _emv_data *_emv_retrieve_data(emv_t, uint16_t id);

/* APDU construction + transactions */
_private int _emv_read_record(emv_t e, uint8_t sfi, uint8_t record);
_private int _emv_select(emv_t e, const uint8_t *name, size_t nlen);
_private int _emv_select_next(emv_t e, const uint8_t *name, size_t nlen);
_private int _emv_verify(emv_t e, uint8_t fmt, const uint8_t *p, uint8_t plen);
_private int _emv_get_data(emv_t e, uint8_t p1, uint8_t p2);
_private int _emv_get_proc_opts(emv_t e, const uint8_t *pdol, uint8_t len);
_private int _emv_generate_ac(emv_t e, uint8_t ref,
				const uint8_t *data, uint8_t len);

_private void _emv_sys_error(struct _emv *e);
_private void _emv_ccid_error(struct _emv *e);
_private void _emv_icc_error(struct _emv *e);
_private void _emv_error(struct _emv *e, unsigned int code);
_private void _emv_success(struct _emv *e);

/* Static data authentication */
//_private int _sda_get_issuer_key(struct _sda *s, RSA *key, size_t key_len);
//_private int _sda_verify_ssa_data(struct _sda *s);

#endif /* _EMV_INTERNAL_H */
