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

static const uint8_t link_rid[] = "\xa0\x00\x00\x00\x29";

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

static int do_sfi1(emv_t e)
{
	static const struct ber_tag tags[] = {
		{ .tag = "\x70", .tag_len = 1, .op = bop_psd1 },
	};
	const uint8_t *res;
	size_t len;

	if ( !_emv_read_record(e, 1, 1) )
		return 0;

	res = xfr_rx_data(e->e_xfr, &len);
	if ( NULL == res )
		return 1;

	ber_decode(tags, sizeof(tags)/sizeof(*tags), res, len, e);

	return 1;
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
static int bop_mac(const uint8_t *ptr, size_t len, void *priv)
{
	printf("MAC: %.2x%.2x-%.2x%.2x-%.2x%.2x-%.2x%.2x\n",
		ptr[8], ptr[9], ptr[10], ptr[11],
		ptr[12], ptr[13], ptr[14], ptr[15]);
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
		{ .tag = "\x8e", .tag_len = 1, .op = bop_mac},
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

static int do_sfi2(emv_t e)
{
	static const struct ber_tag tags[] = {
		{ .tag = "\x70", .tag_len = 1, .op = bop_psd2 },
	};
	const uint8_t *res;
	size_t len;
	unsigned int i;

	for(i = 1; i < 0x10; i++) {
		if ( !_emv_read_record(e, 2, i) )
			return 0;

		res = xfr_rx_data(e->e_xfr, &len);
		if ( NULL == res )
			return 1;

		ber_decode(tags, sizeof(tags)/sizeof(*tags), res, len, e);
	}

	return 1;
}

static int bop_issuer_cert(const uint8_t *ptr, size_t len, void *priv)
{
	printf("Issuer public key certificate:\n");
	hex_dump(ptr, len, 16);
	return 1;
}
static int bop_pk_rem(const uint8_t *ptr, size_t len, void *priv)
{
	printf("Issuer public key remainder:\n");
	hex_dump(ptr, len, 16);
	return 1;
}
static int bop_ssa_data(const uint8_t *ptr, size_t len, void *priv)
{
	printf("Signed SSA data:\n");
	hex_dump(ptr, len, 16);
	return 1;
}
static int bop_pk_exp(const uint8_t *ptr, size_t len, void *priv)
{
	printf("Issuer public key exponent: %u\n", *ptr);
	return 1;
}
static int bop_ca_key_idx(const uint8_t *ptr, size_t len, void *priv)
{
	printf("Certification authority public key index: %u\n", *ptr);
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

static int do_sfi3(emv_t e)
{
	static const struct ber_tag tags[] = {
		{ .tag = "\x70", .tag_len = 1, .op = bop_psd3 },
	};
	const uint8_t *res;
	size_t len;
	unsigned int i;

	for(i = 1; i < 0x10; i++ ) {
		if ( !_emv_read_record(e, 3, i) )
			break;

		res = xfr_rx_data(e->e_xfr, &len);
		if ( NULL == res )
			break;

		ber_decode(tags, sizeof(tags)/sizeof(*tags), res, len, e);
	}

	return 1;
}


int emv_link_init(emv_t e)
{
	struct _emv_app *a, *cur = NULL;
	const uint8_t *res;
	size_t len;

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

	_emv_select(e, e->e_app->a_id, e->e_app->a_id_sz);

	res = xfr_rx_data(e->e_xfr, &len);
	if ( NULL == res )
		return 1;

//	printf("LINK directory descriptor thingy:\n");
//	ber_decode(NULL, 0, res, len, NULL);

	do_sfi1(e);
	do_sfi2(e);
	do_sfi3(e);

	return 1;
}
