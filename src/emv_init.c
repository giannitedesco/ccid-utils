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

static int bop_po(const uint8_t *ptr, size_t len, void *priv)
{
	struct _emv *e = priv;

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

static int get_aip(emv_t e)
{
	static const uint8_t pdol[] = {0x83, 0x00};
	static const struct ber_tag tags[] = {
		{ .tag = "\x80", .tag_len = 1, .op = bop_po},
	};
	const uint8_t *res;
	size_t len;

	if ( !_emv_get_proc_opts(e, pdol, sizeof(pdol)) )
		return 0;
	res = xfr_rx_data(e->e_xfr, &len);
	if ( NULL == res )
		return 0;
	return ber_decode(tags, sizeof(tags)/sizeof(*tags), res, len, e);
}

int _emv_app_init(emv_t e, const uint8_t *aid, size_t aid_len)
{
	static const struct ber_tag tags[] = {
		{ .tag = "\x6f", .tag_len = 1, .op = bop_fci},
	};
	const uint8_t *res;
	size_t len;

	if ( !_emv_select(e, aid, aid_len) ) {
		printf("emv: app not supported\n");
		return 0;
	}

	do {
		res = xfr_rx_data(e->e_xfr, &len);
		if ( NULL == res )
			return 1;

		ber_decode(tags, sizeof(tags)/sizeof(*tags), res, len, e);
	}while( _emv_select_next(e, aid, aid_len) );

	if ( !get_aip(e) )
		return 0;

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

	return 1;
}
