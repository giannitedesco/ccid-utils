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
static void __attribute__((constructor)) link_ctor(void)
{
	keytbl[1].key_len = visa1152_mod_len;
	keytbl[1].key = RSA_new();
	keytbl[1].key->n = BN_bin2bn(visa1152_mod, visa1152_mod_len, NULL);
	keytbl[1].key->e = BN_bin2bn(visa1152_exp, visa1152_exp_len, NULL);
}

static const uint8_t link_rid[] = "\xa0\x00\x00\x00\x03";

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

int emv_link_offline_auth(emv_t e)
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

int emv_link_select(emv_t e)
{
	struct _emv_app *a, *cur = NULL;

	list_for_each_entry(a, &e->e_apps, a_list) {
		if ( !memcmp(a->a_id, link_rid, EMV_RID_LEN) ) {
			if ( NULL == cur || a->a_prio < cur->a_prio )
				cur = a;
		}
	}

	if ( NULL == cur )
		return 0;

	e->e_cur = EMV_APP_LINK;
	e->e_app = cur;

	if ( !_emv_app_init(e, link_rid, EMV_RID_LEN) )
		return 0;

	if ( !_emv_read_app_data(e) )
		return 0;

	return 1;
}

int emv_link_cvm_pin(emv_t e, const char *pin)
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

	if ( _emv_verify(e, 0x80, pb, sizeof(pb)) )
		return 1;

	printf("PIN auth failed");
	if ( xfr_rx_sw1(e->e_xfr) == 0x63)
		printf(" with %u tries remaining",
			xfr_rx_sw2(e->e_xfr));
	printf("\n");
	return 0;
}
