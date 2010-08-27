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

static int get_aip(emv_t e)
{
	static const uint8_t pdol[] = {0x83, 0x00};
	struct gber_tag tag;
	const uint8_t *res;
	size_t len;

	/* TODO: handle case where PDOL was specified */
	if ( !_emv_get_proc_opts(e, pdol, sizeof(pdol)) )
		return 0;

	res = xfr_rx_data(e->e_xfr, &len);
	if ( NULL == res )
		return 0;

	res = ber_decode_tag(&tag, res, len);
	if ( NULL == res ) {
		_emv_error(e, EMV_ERR_BER_DECODE);
		return 0;
	}

	if ( tag.ber_len < sizeof(e->e_aip) ) {
		_emv_error(e, EMV_ERR_BER_DECODE);
		return 0;
	}

	memcpy(e->e_aip, res, sizeof(e->e_aip));

	e->e_afl = malloc(e->e_afl_len);
	if ( NULL == e->e_afl ) {
		_emv_sys_error(e);
		return 0;
	}

	e->e_afl_len = tag.ber_len - sizeof(e->e_aip);
	memcpy(e->e_afl, res + sizeof(e->e_aip), e->e_afl_len);
	return 1;
}

int emv_app_init(emv_t e)
{
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

	_emv_success(e);
	return 1;
}
