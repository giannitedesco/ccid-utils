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

		printf(" { %.2x %.2x } ", ptr[0], ptr[1]);
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
	struct _emv *e = priv;
	printf("CDOL1:\n");
	hex_dump(ptr, len, 16);
	e->e_cdol1 = malloc(len);
	assert(NULL != e->e_cdol1);
	memcpy(e->e_cdol1, ptr, len);
	e->e_cdol1_len = len;
	return 1;
}
static int bop_cdol2(const uint8_t *ptr, size_t len, void *priv)
{
	struct _emv *e = priv;
	printf("CDOL2:\n");
	hex_dump(ptr, len, 16);
	e->e_cdol2 = malloc(len);
	assert(NULL != e->e_cdol2);
	memcpy(e->e_cdol2, ptr, len);
	e->e_cdol2_len = len;
	return 1;
}

static int bop_sda_tags(const uint8_t *ptr, size_t len, void *priv)
{
	printf("Static data authentication tag list:\n");
	hex_dump(ptr, len, 16);
	return 1;
}

static int bop_issuer_cert(const uint8_t *ptr, size_t len, void *priv)
{
	struct _emv *e = priv;
	//printf("Issuer public key certificate:\n");
	//hex_dump(ptr, len, 16);
	e->e_sda.iss_cert = malloc(len);
	assert(NULL != e->e_sda.iss_cert);
	memcpy(e->e_sda.iss_cert, ptr, len);
	e->e_sda.iss_cert_len = len;
	return 1;
}
static int bop_pk_rem(const uint8_t *ptr, size_t len, void *priv)
{
	struct _emv *e = priv;
	//printf("Issuer public key remainder:\n");
	//hex_dump(ptr, len, 16);
	e->e_sda.iss_pubkey_r = malloc(len);
	assert(NULL != e->e_sda.iss_pubkey_r);
	memcpy(e->e_sda.iss_pubkey_r, ptr, len);
	e->e_sda.iss_pubkey_r_len = len;
	return 1;
}
static int bop_ssa_data(const uint8_t *ptr, size_t len, void *priv)
{
	struct _emv *e = priv;
	//printf("Signed SSA data:\n");
	//hex_dump(ptr, len, 16);
	e->e_sda.ssa_data = malloc(len);
	assert(NULL != e->e_sda.ssa_data);
	memcpy(e->e_sda.ssa_data, ptr, len);
	e->e_sda.ssa_data_len = len;
	return 1;
}
static int bop_pk_exp(const uint8_t *ptr, size_t len, void *priv)
{
	struct _emv *e = priv;
	//printf("Issuer public key exponent:\n");
	//hex_dump(ptr, len, 16);
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

static int bop_psd(const uint8_t *ptr, size_t len, void *priv)
{
	static const struct ber_tag tags[] = {
		{ .tag = "\x57", .tag_len = 1, .op = bop_track2},
		{ .tag = "\x5a", .tag_len = 1, .op = bop_pan},
		{ .tag = "\x8c", .tag_len = 1, .op = bop_cdol1},
		{ .tag = "\x8d", .tag_len = 1, .op = bop_cdol2},
		{ .tag = "\x8e", .tag_len = 1, .op = bop_cvm},
		{ .tag = "\x8f", .tag_len = 1, .op = bop_ca_key_idx},
		{ .tag = "\x90", .tag_len = 1, .op = bop_issuer_cert},
		{ .tag = "\x92", .tag_len = 1, .op = bop_pk_rem},
		{ .tag = "\x93", .tag_len = 1, .op = bop_ssa_data},
		{ .tag = "\x5f\x20", .tag_len = 2, .op = bop_cardholder},
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
		{ .tag = "\x9f\x1f", .tag_len = 2, .op = bop_track1d},
		{ .tag = "\x9f\x32", .tag_len = 2, .op = bop_pk_exp},
		{ .tag = "\x9f\x4a", .tag_len = 2, .op = bop_sda_tags},
	};
	return ber_decode(tags, sizeof(tags)/sizeof(*tags), ptr, len, priv);
}

static int read_sfi(emv_t e, uint8_t sfi, uint8_t begin, uint8_t end)
{
	static const struct ber_tag tags[] = {
		{ .tag = "\x70", .tag_len = 1, .op = bop_psd},
	};
	const uint8_t *res;
	size_t len;
	unsigned int i;

	printf("Reading SFI %u records %u - %u\n", sfi, begin, end);
	for(i = begin; i <= end; i++ ) {
		if ( !_emv_read_record(e, sfi, i) )
			break;

		res = xfr_rx_data(e->e_xfr, &len);
		if ( NULL == res )
			break;

		ber_decode(tags, sizeof(tags)/sizeof(*tags), res, len, e);
		if ( begin != 1 )
			hex_dump(res, len, 16);
	}

	return 1;
}

int _emv_read_app_data(struct _emv *e)
{
	uint8_t *ptr, *end;

	for(ptr = e->e_afl, end = e->e_afl + e->e_afl_len;
		ptr + 4 <= end; ptr += 4) {
		read_sfi(e, ptr[0] >> 3, ptr[1], ptr[2]);
	}

	return 1;
}

static int bop_cnt(const uint8_t *ptr, size_t len, void *priv)
{
	size_t *cnt = priv;
	(*cnt) += len;
	return 1;
}

static int bop_cpy(const uint8_t *ptr, size_t len, void *priv)
{
	uint8_t **buf = (uint8_t **)priv;

	if ( buf[0] + len > buf[1] ) {
		return 0;
	}

	memcpy(buf[0], ptr, len);
	buf[0] += len;
	return 1;
}

static int read_sda_data(emv_t e, uint8_t sfi, uint8_t begin, uint8_t end)
{
	static const struct ber_tag tags[] = {
		{ .tag = "\x70", .tag_len = 1, .op = bop_cnt},
	};
	static const struct ber_tag tags2[] = {
		{ .tag = "\x70", .tag_len = 1, .op = bop_cpy},
	};
	const uint8_t *res;
	uint8_t *sda[2];
	uint8_t *buf;
	size_t len, cnt = 0;
	unsigned int i;

	printf("SDA data in SFI %u records %u - %u\n", sfi, begin, end);
	for(i = begin; i <= end; i++ ) {
		if ( !_emv_read_record(e, sfi, i) )
			break;

		res = xfr_rx_data(e->e_xfr, &len);
		if ( NULL == res )
			break;

		ber_decode(tags, sizeof(tags)/sizeof(*tags), res, len, &cnt);
	}

	cnt += sizeof(e->e_aip);

	sda[0] = buf = malloc(cnt);
	sda[1] = sda[0] + cnt;

	for(i = begin; i <= end; i++ ) {
		if ( !_emv_read_record(e, sfi, i) )
			break;

		res = xfr_rx_data(e->e_xfr, &len);
		if ( NULL == res )
			break;

		ber_decode(tags2, sizeof(tags2)/sizeof(*tags2), res, len, &sda);
	}

	if ( sda[0] + sizeof(e->e_aip) > sda[1] )
		return 0;
	
	/* FIXME: this is fucked if SDA data split across multiple SFI's */
	memcpy(sda[0], e->e_aip, sizeof(e->e_aip));

	e->e_sda.data = buf;
	e->e_sda.data_len = cnt;

	return 1;
}

int _emv_read_sda_data(struct _emv *e)
{
	uint8_t *ptr, *end;

	for(ptr = e->e_afl, end = e->e_afl + e->e_afl_len;
		ptr + 4 <= end; ptr += 4) {
		if ( !ptr[3] )
			continue;
		read_sda_data(e, ptr[0] >> 3, ptr[1], (ptr[1] + ptr[3]) - 1);
	}

	return 1;
}

static int bop_ptc(const uint8_t *ptr, size_t len, void *priv)
{
	int *ctr = priv;
	assert(1 == len);
	*ctr = *ptr;
	return 1;
}

int _emv_pin_try_counter(struct _emv *e)
{
	static const struct ber_tag tags[] = {
		{ .tag = "\x9f\x17", .tag_len = 2, .op = bop_ptc},
	};
	const uint8_t *ptr;
	size_t len;
	int ctr = -1;

	if ( !_emv_get_data(e, 0x9f, 0x17) )
		return -1;
	ptr = xfr_rx_data(e->e_xfr, &len);
	if ( NULL == ptr )
		return -1;
	if ( !ber_decode(tags, sizeof(tags)/sizeof(*tags), ptr, len, &ctr) )
		return -1;
	return ctr;
}

