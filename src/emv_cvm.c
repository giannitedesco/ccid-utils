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

static int bop_ptc(const uint8_t *ptr, size_t len, void *priv)
{
	int *ctr = priv;
	*ctr = *ptr;
	return 1;
}

static int ptc(struct _emv *e)
{
	static const struct ber_tag tags[] = {
		{ .tag = "\x9f\x17", .tag_len = 2, .op = bop_ptc},
	};
	const uint8_t *ptr;
	size_t len;
	int ctr = -1;

	if ( !_emv_get_data(e, 0x9f, 0x17) ) {
		_emv_error(e, EMV_ERR_DATA_ELEMENT_NOT_FOUND);
		return -1;
	}
	ptr = xfr_rx_data(e->e_xfr, &len);
	if ( NULL == ptr ) {
		_emv_error(e, EMV_ERR_DATA_ELEMENT_NOT_FOUND);
		return -1;
	}
	if ( !ber_decode(tags, sizeof(tags)/sizeof(*tags), ptr, len, &ctr) ) {
		_emv_error(e, EMV_ERR_DATA_ELEMENT_NOT_FOUND);
		return -1;
	}
	if ( ctr < 0 ) {
		_emv_error(e, EMV_ERR_BER_DECODE);
		return -1;
	}
	return ctr;
}

int emv_pin_try_counter(struct _emv *e)
{
	return ptc(e);
}

int emv_cvm_pin(emv_t e, const char *pin)
{
	emv_pb_t pb;
	int try;

	if ( !_emv_pin2pb(pin, pb) ) {
		_emv_error(e, EMV_ERR_BAD_PIN_FORMAT);
		return 0;
	}

	try = ptc(e);
	if ( _emv_verify(e, 0x80, pb, sizeof(pb)) )
		return 1;

	if ( _emv_sw1(e) == 0x63 )
		_emv_error(e, EMV_ERR_BAD_PIN);

	return 0;
}

int emv_cvm(emv_t e)
{
	const struct _emv_data *d;
	const uint8_t *ptr;
	size_t len;

	d = _emv_retrieve_data(e, EMV_TAG_CVM_LIST);
	if ( NULL == d ) {
		_emv_error(e, EMV_ERR_DATA_ELEMENT_NOT_FOUND);
		return -1;
	}

	ptr = d->d_data;
	len = d->d_len;

	printf("Cardholder verification methods:\n");
	hex_dump(ptr, len, 16);

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
