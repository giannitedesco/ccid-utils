/*
 * This file is part of cci-utils
 * Copyright (c) 2010 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
 *
 * Chip card transaction buffer management.
*/

#include <ccid.h>

#include "ccid-internal.h"

static int enable_cmrc632(struct _cci *cci)
{
	xfr_reset(cci->cci_xfr);
	xfr_tx_byte(cci->cci_xfr, 0x1);
	if ( !_PC_to_RDR_Escape(cci, 1, cci->cci_xfr) )
		return 0;
	_RDR_to_PC(cci, 1, cci->cci_xfr);
	/* should return error code 5 and empty buffer */
	trace(cci, " o CMRC632 RF interface ASIC enabled (%u/%zu)\n",
		cci->cci_xfr->x_rxhdr->in.bError, cci->cci_xfr->x_rxlen);
	return 1;
}

void _omnikey_init_prox(struct _cci *cci)
{
	trace(cci, " o Omnikey proxcard RF interface detected\n");
	if ( !enable_cmrc632(cci) )
		return;
}
