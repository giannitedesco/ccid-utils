/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>
#include <list.h>
#include <emv.h>
#include <gber.h>
#include "emv-internal.h"

static int parse_format1(emv_t e, const uint8_t *ptr, size_t len)
{
	if ( len < sizeof(e->e_aip) )
		return 9;

	memcpy(e->e_aip, ptr, sizeof(e->e_aip));

	e->e_afl_len = len - sizeof(e->e_aip);
	e->e_afl = malloc(e->e_afl_len);
	if ( NULL == e->e_afl )
		return 0;

	memcpy(e->e_afl, ptr + sizeof(e->e_aip), e->e_afl_len);

	return 1;
}

static int parse_format2(emv_t e, const uint8_t *ptr, size_t len)
{
	const uint8_t *inner, *end = ptr + len;
	struct gber_tag tag;

again:
	inner = ber_decode_block(&tag, ptr, len);
	if ( NULL == inner ) {
		_emv_error(e, EMV_ERR_BER_DECODE);
		return 0;
	}

	switch(tag.ber_tag) {
	case 0x82:
		memcpy(e->e_aip, inner, sizeof(e->e_aip));
		break;
	case 0x94:
		e->e_afl_len = tag.ber_len;
		e->e_afl = malloc(tag.ber_len);
		if ( NULL == e->e_afl )
			return 0;
		memcpy(e->e_afl, inner, tag.ber_len);
		break;
	default:
		break;
	}

	inner += tag.ber_len;
	if ( inner < end ) {
		ptr = inner;
		len = end - ptr;
		goto again;
	}

	return 1;
}

static int get_aip(emv_t e)
{
	static const uint8_t pdol[] = {0x83, 0x00};
	const uint8_t *res, *inner;
	struct gber_tag tag;
	size_t len;

	/* TODO: handle case where PDOL was specified */

	if ( !_emv_get_proc_opts(e, pdol, sizeof(pdol)) ) {
		return 0;
	}
	res = xfr_rx_data(e->e_xfr, &len);
	if ( NULL == res )
		return 0;

	inner = ber_decode_block(&tag, res, len);
	if ( NULL == inner ) {
		_emv_error(e, EMV_ERR_BER_DECODE);
		return 0;
	}

	switch(tag.ber_tag) {
	case 0x80:
		return parse_format1(e, inner, tag.ber_len);
	case 0x77:
		return parse_format2(e, inner, tag.ber_len);
	}

	return 1;
}

int emv_app_init(emv_t e)
{
	if ( !get_aip(e) ) {
		return 0;
	}

#if 1
	if ( e->e_aip[0] & EMV_AIP_ISS )
		printf("AIP: Issuer authentication\n");
	if ( e->e_aip[0] & EMV_AIP_TRM )
		printf("AIP: Terminal risk management required\n");
	if ( e->e_aip[0] & EMV_AIP_CVM )
		printf("AIP: Cardholder verification supported\n");
	if ( e->e_aip[0] & EMV_AIP_DDA )
		printf("AIP: Dynamic data authentication\n");
	if ( e->e_aip[0] & EMV_AIP_SDA)
		printf("AIP: Static data authentication\n");
	if ( e->e_aip[0] & EMV_AIP_CDA)
		printf("AIP: Combined data authentication\n");
#endif

	_emv_success(e);
	return 1;
}

int emv_app_aip(emv_t e, emv_aip_t aip)
{
	if ( NULL == e->e_app ) {
		_emv_error(e, EMV_ERR_APP_NOT_SELECTED);
		return 0;
	}

	memcpy(aip, e->e_aip, sizeof(e->e_aip));
	return 1;
}
