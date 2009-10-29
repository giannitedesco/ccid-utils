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

#include "ca_pubkeys.h"

#include <ctype.h>

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

static RSA *find_ca_pubkey(struct _sda *s, size_t *key_len)
{
	unsigned int i;
	RSA *key = NULL;

	for(i = 0; i < num_keys; i++) {
		if ( keytbl[i].idx == s->key_idx ) {
			key = keytbl[i].key;
			*key_len = keytbl[i].key_len;
			break;
		}
	}

	if ( NULL == key ) {
		printf("error: key %u not found\n", s->key_idx);
		return 0;
	}

	return key;
}

int emv_visa_offline_auth(emv_t e)
{
	RSA *key;
	size_t key_len;

	/* This terminal only supports offline auth heh */

	if ( 0 == (e->e_aip[0] & EMV_AIP_SDA) ) {
		printf("error: SDA not supported, auth failed\n");
		return 0;
	}

	key = find_ca_pubkey(&e->e_sda, &key_len);
	if ( NULL == key )
		return 0;
	if ( !_sda_get_issuer_key(&e->e_sda, key, key_len) )
		return 0;
	if ( !_emv_read_sda_data(e) )
		return 0;
	if ( !_sda_verify_ssa_data(&e->e_sda) )
		return 0;
	
	printf("Card is authorized\n");
	return 1;
}

int emv_visa_select(emv_t e)
{
	struct _emv_app *a, *cur = NULL;

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

	if ( !_emv_app_init(e, visa_rid, EMV_RID_LEN) )
		return 0;

	if ( !_emv_read_app_data(e) )
		return 0;

	return 1;
}

#if 1
static int dop_arc(uint8_t *ptr, size_t len, void *priv)
{
	printf(" Authorization response code: %u bytes\n", len);
	return 0;
}

static int dop_tvr(uint8_t *ptr, size_t len, void *priv)
{
	printf(" Terminal verification results\n");
	if ( 0 ) {
		/* online pin entered */
		memset(ptr, 0, len);
		ptr[2] = 0x4;
		return 1;
	}else
		return 0;
}

static int dop_date(uint8_t *ptr, size_t len, void *priv)
{
	if ( len != 3 )
		return 0;
	ptr[0] = 0x09;
	ptr[1] = 0x10;
	ptr[2] = 0x28;
	printf(" Transaction date: 20%.2x-%.2x-%.2x\n",
		ptr[0], ptr[1], ptr[2]);
	return 1;
}

static int dop_currency(uint8_t *ptr, size_t len, void *priv)
{
	printf(" Transaction currency code: %u bytes\n", len);
	return 0;
}

static int dop_amount(uint8_t *ptr, size_t len, void *priv)
{
	printf(" Transaction amount authorized: %u bytes\n", len);
	return 0;
}

static int dop_amt_other(uint8_t *ptr, size_t len, void *priv)
{
	printf(" Transaction amount (other): %u bytes\n", len);
	return 0;
}

static int dop_country(uint8_t *ptr, size_t len, void *priv)
{
	printf(" Terminal country code: %u bytes\n", len);
	ptr[0] = 0x08;
	ptr[1] = 0x26;
	return 1;
}

static int dop_rand(uint8_t *ptr, size_t len, void *priv)
{
	printf(" Unpredictable number: %u bytes\n", len);
	return 0;
}

static int dop_type(uint8_t *ptr, size_t len, void *priv)
{
	printf(" Transaction type: %u bytes\n", len);
	return 0;
}

static const struct dol_tag trans_data[] = {
	{.tag = "\x8a", .tag_len = 1, .op = dop_arc },
	{.tag = "\x95", .tag_len = 1, .op = dop_tvr },
	{.tag = "\x9a", .tag_len = 1, .op = dop_date },
	{.tag = "\x9c", .tag_len = 1, .op = dop_type },
	{.tag = "\x5f\x2a", .tag_len = 2, .op = dop_currency },
	{.tag = "\x9f\x02", .tag_len = 2, .op = dop_amount },
	{.tag = "\x9f\x03", .tag_len = 2, .op = dop_amt_other },
	{.tag = "\x9f\x1a", .tag_len = 2, .op = dop_country },
	{.tag = "\x9f\x37", .tag_len = 2, .op = dop_rand },
};

static int decode_ac(const uint8_t *ptr, size_t len)
{
	size_t ac_len;

	if ( ptr[0] != 0x80 ) {
		printf("error: invalid AC response\n");
		return 0;
	}

	ac_len = ptr[1] - 2;
	if ( ptr[1] < 2 || ac_len + 4 > len ) {
		printf("error: bad tag len in AC\n");
		return 0;
	}

	/* Cryptogram information data */
	switch(ptr[2] & 0xc0) {
	case EMV_AC_AAC:
		printf("AAC: Transaction declined\n");
		break;
	case EMV_AC_TC:
		printf("TC: Transaction approved\n");
		break;
	case EMV_AC_ARQC:
		printf("ARQC: Online processing requested\n");
		break;
	default:
		printf("RFU\n");
		return 0;
	}

	/* application trans. counter */
	printf("Transaction counter: %u\n", ptr[3]);

	printf("Application cryptogram:\n");
	hex_dump(ptr + 4, ac_len, 16);

	if ( ac_len + 4 < len ) {
		printf("Optional issuer application data:\n");
		hex_dump(ptr + ac_len + 4, len - (ac_len + 4), 16);
	}

	return 1;
}
static int arqc(emv_t e)
{
	const uint8_t *res;
	uint8_t *dol;
	size_t len;
	int ret;

	printf("Sending ARQC:\n");
	dol = _emv_construct_dol(trans_data, DOL_NUM_TAGS(trans_data),
				e->e_cdol1, e->e_cdol1_len,
				&len, e);
	if ( NULL == dol )
		return 0;

	hex_dump(dol, len, 16);
	ret = _emv_generate_ac(e, EMV_AC_ARQC, dol, len);
	free(dol);

	if ( !ret ) {
		printf("Cryptogram not generated\n");
		return 0;
	}

	res = xfr_rx_data(e->e_xfr, &len);
	return decode_ac(res, len);
}

static int tc(emv_t e)
{
	const uint8_t *res;
	uint8_t *dol;
	size_t len;
	int ret;

	printf("Sending TC req:\n");
	dol = _emv_construct_dol(trans_data, DOL_NUM_TAGS(trans_data),
				e->e_cdol2, e->e_cdol2_len,
				&len, e);
	if ( NULL == dol )
		return 0;

	hex_dump(dol, len, 16);
	ret = _emv_generate_ac(e, EMV_AC_TC, dol, len);
	free(dol);

	if ( !ret ) {
		printf("Cryptogram not generated\n");
		return 0;
	}

	printf("Received TC:\n");
	res = xfr_rx_data(e->e_xfr, &len);
	return decode_ac(res, len);
}
#endif

int emv_visa_cvm_pin(emv_t e, const char *pin)
{
	emv_pb_t pb;
	int try;

	if ( !_emv_pin2pb(pin, pb) ) {
		printf("error: invalid PIN\n");
		return 0;
	}

	try = _emv_pin_try_counter(e);
	if ( try >= 0 )
		printf("%i PIN tries remaining\n", try);

	if ( !_emv_verify(e, 0x80, pb, sizeof(pb)) ) {
		printf("PIN auth failed");
		if ( xfr_rx_sw1(e->e_xfr) == 0x63)
			printf(" with %u tries remaining",
				xfr_rx_sw2(e->e_xfr));
		printf("\n");
		return 1;
	}

	if ( arqc(e) )
		tc(e);

	return 1;
}
