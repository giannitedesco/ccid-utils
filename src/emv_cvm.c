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

static int ptc(struct _emv *e)
{
	const uint8_t *ptr;
	size_t len;
	int ctr = -1;

	if ( !_emv_get_data(e, 0x9f, 0x17) )
		return ctr;
	ptr = xfr_rx_data(e->e_xfr, &len);
	if ( NULL == ptr )
		return ctr;

	_emv_error(e, EMV_ERR_DATA_ELEMENT_NOT_FOUND);
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
