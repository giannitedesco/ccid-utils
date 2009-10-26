/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>
#include <list.h>
#include <emv.h>
#include <ber.h>
#include "emv-internal.h"

#include "visa_pubkeys.h"

static struct {
	unsigned int idx;
	size_t key_len;
	RSA *key;
}keytbl[] = {
	{.idx = 1, }, /* 1024 bit key / 2009 */
	{.idx = 7, }, /* 1152 bit key / 2015 */
	{.idx = 8, }, /* 1408 bit key / 2018 */
	{.idx = 9, }, /* 1984 bit key / 2018 */
};
static const unsigned int num_keys = sizeof(keytbl)/sizeof(*keytbl);
static void __attribute__((constructor)) visa_ctor(void)
{
	keytbl[1].key_len = visa1152_mod_len;
	keytbl[1].key = RSA_new();
	keytbl[1].key->n = BN_bin2bn(visa1152_mod, visa1152_mod_len, NULL);
	keytbl[1].key->e = BN_bin2bn(visa1152_exp, visa1152_exp_len, NULL);
}

static const uint8_t visa_rid[] = "\xa0\x00\x00\x00\x03";

static int bop_cardholder(const uint8_t *ptr, size_t len, void *priv)
{
	printf("Cardholder name: %.*s\n", len, ptr);
	return 1;
}
static int bop_track2(const uint8_t *ptr, size_t len, void *priv)
{
	printf("Track 2 magstrip equivalent:\n");
	hex_dump(ptr, len, 16);
	return 1;
}
static int bop_track1d(const uint8_t *ptr, size_t len, void *priv)
{
	printf("Track 1 magstrip discretionary data:\n");
	hex_dump(ptr, len, 16);
	return 1;
}
static int bop_psd1(const uint8_t *ptr, size_t len, void *priv)
{
	static const struct ber_tag tags[] = {
		{ .tag = "\x57", .tag_len = 1, .op = bop_track2},
		{ .tag = "\x5f\x20", .tag_len = 2, .op = bop_cardholder},
		{ .tag = "\x9f\x1f", .tag_len = 2, .op = bop_track1d},
	};
	return ber_decode(tags, sizeof(tags)/sizeof(*tags), ptr, len, priv);
}

static int bop_exp_date(const uint8_t *ptr, size_t len, void *priv)
{
	assert(len >= 3);
	printf("Expiration date: 20%.2x-%.2x-%.2x\n",
		ptr[0], ptr[1], ptr[2]);
	return 1;
}
static int bop_eff_date(const uint8_t *ptr, size_t len, void *priv)
{
	assert(len >= 3);
	printf("Effective date: 20%.2x-%.2x-%.2x\n",
		ptr[0], ptr[1], ptr[2]);
	return 1;
}
static int bop_pan(const uint8_t *ptr, size_t len, void *priv)
{
	assert(len >= 8);
	printf("PAN: %.2x%.2x %.2x%.2x %.2x%.2x %.2x%.2x\n",
		ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6], ptr[7]);
	return 1;
}
static int bop_cvm(const uint8_t *ptr, size_t len, void *priv)
{
	printf("Cardholder verification methods:\n");
	//hex_dump(ptr, len, 16);

	if ( len <= 8 )
		return 1;
	
	for(ptr += 8, len -= 8; len >= 2; ptr += 2, len -= 2) {
		switch(ptr[0] & 0x3f) {
		case 0:
			printf(" o Fail CVM processing");
			break;
		case 1:
			printf(" o Plaintext offline PIN");
			break;
		case 2:
			printf(" o Enciphered online PIN");
			break;
		case 3:
			printf(" o Plaintext offline PIN + paperr sig");
			break;
		case 4:
			printf(" o Enciphered offline PIN");
			break;
		case 5:
			printf(" o Enciphered offline PIN + paper sig");
			break;
		case 0x3e:
			printf(" o Paper signature");
			break;
		case 0x3f:
			printf(" o No CVM required");
			break;
		default:
			printf(" o Proprietary/RFU CVM (%.2x)",
				ptr[0] & 0x3f);
			break;
		}

		switch(ptr[1]) {
		case 0:
			printf(" always");
			break;
		case 1:
			printf(" if unattended cash");
			break;
		case 2:
			printf(" if not cash");
			break;
		case 3:
			printf(" if terminal supports it");
			break;
		case 4:
			printf(" if manual cash");
			break;
		case 5:
			printf(" if purchase with cashback");
			break;
		case 6:
			printf(" for cash < X in app currency");
			break;
		case 7:
			printf(" for cash > X in app currency");
			break;
		case 8:
			printf(" for cash < Y in app currency");
			break;
		case 9:
			printf(" for cash > Y in app currency");
			break;
		default:
			break;
		}

		if ( ptr[0] & 0x40 ) {
			printf("\n");
		}else{
			printf(" [TERMINATE]\n");
		}
	}

	printf("\n");
	return 1;
}

/* List of tags for GENERATE AC commands */
static int bop_cdol1(const uint8_t *ptr, size_t len, void *priv)
{
	printf("CDOL1\n");
	hex_dump(ptr, len, 16);
	return 1;
}
static int bop_cdol2(const uint8_t *ptr, size_t len, void *priv)
{
	printf("CDOL2\n");
	hex_dump(ptr, len, 16);
	return 1;
}

static int bop_sda_tags(const uint8_t *ptr, size_t len, void *priv)
{
	printf("Static data authentication tag list:\n");
	hex_dump(ptr, len, 16);
	return 1;
}

static int bop_psd2(const uint8_t *ptr, size_t len, void *priv)
{
	static const struct ber_tag tags[] = {
		{ .tag = "\x5a", .tag_len = 1, .op = bop_pan},
		{ .tag = "\x8c", .tag_len = 1, .op = bop_cdol1},
		{ .tag = "\x8d", .tag_len = 1, .op = bop_cdol2},
		{ .tag = "\x8e", .tag_len = 1, .op = bop_cvm},
		{ .tag = "\x5f\x24", .tag_len = 2, .op = bop_exp_date},
		{ .tag = "\x5f\x25", .tag_len = 2, .op = bop_eff_date},
		{ .tag = "\x5f\x28", .tag_len = 2, /* country code */ },
		{ .tag = "\x5f\x30", .tag_len = 2, /* service code */ },
		{ .tag = "\x5f\x34", .tag_len = 2, /* PAN sequence */},
		{ .tag = "\x9f\x07", .tag_len = 2, /* usage control */},
		{ .tag = "\x9f\x08", .tag_len = 2, /* version number */},
		{ .tag = "\x9f\x0d", .tag_len = 2, /* default action */},
		{ .tag = "\x9f\x0e", .tag_len = 2, /* deny action */},
		{ .tag = "\x9f\x0f", .tag_len = 2, /* online action */},
		{ .tag = "\x9f\x4a", .tag_len = 2, .op = bop_sda_tags},
	};
	return ber_decode(tags, sizeof(tags)/sizeof(*tags), ptr, len, priv);
}

static int bop_issuer_cert(const uint8_t *ptr, size_t len, void *priv)
{
	struct _emv *e = priv;
	printf("Issuer public key certificate:\n");
	hex_dump(ptr, len, 16);
	e->e_sda.iss_cert = malloc(len);
	assert(NULL != e->e_sda.iss_cert);
	memcpy(e->e_sda.iss_cert, ptr, len);
	e->e_sda.iss_cert_len = len;
	return 1;
}
static int bop_pk_rem(const uint8_t *ptr, size_t len, void *priv)
{
	struct _emv *e = priv;
	printf("Issuer public key remainder:\n");
	hex_dump(ptr, len, 16);
	e->e_sda.iss_pubkey_r = malloc(len);
	assert(NULL != e->e_sda.iss_pubkey_r);
	memcpy(e->e_sda.iss_pubkey_r, ptr, len);
	e->e_sda.iss_pubkey_r_len = len;
	return 1;
}
static int bop_ssa_data(const uint8_t *ptr, size_t len, void *priv)
{
	struct _emv *e = priv;
	printf("Signed SSA data:\n");
	hex_dump(ptr, len, 16);
	e->e_sda.ssa_data = malloc(len);
	assert(NULL != e->e_sda.ssa_data);
	memcpy(e->e_sda.ssa_data, ptr, len);
	e->e_sda.ssa_data_len = len;
	return 1;
}
static int bop_pk_exp(const uint8_t *ptr, size_t len, void *priv)
{
	struct _emv *e = priv;
	printf("Issuer public key exponent:\n");
	hex_dump(ptr, len, 16);
	e->e_sda.iss_exp = malloc(len);
	assert(NULL != e->e_sda.iss_exp);
	memcpy(e->e_sda.iss_exp, ptr, len);
	e->e_sda.iss_exp_len = len;
	return 1;
}
static int bop_ca_key_idx(const uint8_t *ptr, size_t len, void *priv)
{
	struct _emv *e = priv;
	assert(1 == len);
	printf("Certification authority public key index: %u\n", *ptr);
	e->e_sda.key_idx = *ptr;
	return 1;
}

static int bop_psd3(const uint8_t *ptr, size_t len, void *priv)
{
	static const struct ber_tag tags[] = {
		{ .tag = "\x8f", .tag_len = 1, .op = bop_ca_key_idx},
		{ .tag = "\x90", .tag_len = 1, .op = bop_issuer_cert},
		{ .tag = "\x92", .tag_len = 1, .op = bop_pk_rem},
		{ .tag = "\x93", .tag_len = 1, .op = bop_ssa_data},
		{ .tag = "\x9f\x32", .tag_len = 2, .op = bop_pk_exp},
	};
	return ber_decode(tags, sizeof(tags)/sizeof(*tags), ptr, len, priv);
}

static int rinse_sfi(emv_t e, unsigned int sfi,
			int (*bop)(const uint8_t *, size_t, void *))
{
	struct ber_tag tags[] = {
		{ .tag = "\x70", .tag_len = 1, .op = bop},
	};
	const uint8_t *res;
	size_t len;
	unsigned int i;

	for(i = 1; i < 0x10; i++ ) {
		if ( !_emv_read_record(e, sfi, i) )
			break;

		res = xfr_rx_data(e->e_xfr, &len);
		if ( NULL == res )
			break;

		ber_decode(tags, sizeof(tags)/sizeof(*tags), res, len, e);
	}

	return 1;
}

static int get_issuer_key(struct _sda *s)
{
	unsigned int i;
	size_t key_len;
	RSA *key = NULL;

	for(i = 0; i < num_keys; i++) {
		if ( keytbl[i].idx == s->key_idx ) {
			key = keytbl[i].key;
			key_len = keytbl[i].key_len;
			break;
		}
	}

	if ( NULL == key ) {
		printf("error: key %u not found\n", s->key_idx);
		return 0;
	}
	
	return _sda_get_issuer_key(s, key, key_len);
}

int emv_visa_init(emv_t e)
{
	struct _emv_app *a, *cur = NULL;
	const uint8_t *res;
	size_t len;

	list_for_each_entry(a, &e->e_apps, a_list) {
		if ( !memcmp(a->a_id, visa_rid, EMV_RID_LEN) ) {
			if ( NULL == cur || a->a_prio < cur->a_prio )
				cur = a;
		}
	}

	if ( NULL == cur )
		return 0;

	e->e_cur = EMV_APP_VISA;
	e->e_app = cur;

	_emv_select(e, e->e_app->a_id, e->e_app->a_id_sz);

	res = xfr_rx_data(e->e_xfr, &len);
	if ( NULL == res )
		return 1;

//	printf("VISA directory descriptor thingy:\n");
//	ber_decode(NULL, 0, res, len, NULL);

	rinse_sfi(e, 1, bop_psd1);
	rinse_sfi(e, 2, bop_psd2);
	rinse_sfi(e, 3, bop_psd3);

	if ( get_issuer_key(&e->e_sda) )
		_sda_verify_ssa_data(&e->e_sda);

	return 1;
}

int emv_visa_pin(emv_t emv, char *pin)
{
	uint8_t pb[8];
	size_t plen;
	unsigned int i;

	plen = strlen(pin);
	if ( plen < 4 || plen > 12 )
		return 0;

	memset(pb, 0xff, sizeof(pb));

	pb[0] = 0x20 | (plen & 0xf);
	for(i = 0; pin[i]; i++) {
		if ( !isdigit(pin[i]) )
			return 0;
		if ( i & 0x1 ) {
			pb[1 + (i >> 1)] = (pb[1 + (i >> 1)] & 0xf0) |
					((pin[i] - '0') & 0xf);
		}else{
			pb[1 + (i >> 1)] = ((pin[i] - '0') << 4) | 0xf;
		}
	}

	hex_dump(pb, sizeof(pb), 16);
	return _emv_verify(emv, 0x80, pb, sizeof(pb));
}
