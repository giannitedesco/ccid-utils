/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
 *
 * Interface to a chipcard rfid interface/slot.
*/

#include <ccid.h>

#include "ccid-internal.h"
#include "rfid.h"
#include "clrc632.h"

/* TODO: Need tag selection API whereby we do collision detection, allow
 * caller to determine which tag they want to talk to and then returns
 * it's status and type. Would be stubbed out with a 'contact' type in the
 * regular smart-card case
*/

static const uint8_t *rfid_power_on(struct _cci *cci, unsigned int voltage,
				size_t *atr_len)
{
	struct _ccid *ccid = cci->cc_parent;
	if ( !_clrc632_rf_power(cci, 1) )
		return NULL;
	if ( !_clrc632_14443a_init(cci) )
		return NULL;
	if ( !_rfid_select(cci) )
		return NULL;
	if ( atr_len )
		*atr_len = ccid->cci_xfr->x_rxlen;
	return ccid->cci_xfr->x_rxbuf;
}

static int rfid_power_off(struct _cci *cci)
{
	return _clrc632_rf_power(cci, 0);
}

static int rfid_transact(struct _cci *cci, struct _xfr *xfr)
{
	printf("%s: called\n", __func__);
	return 0;
}

_private const struct _cci_ops _rfid_ops = {
	.power_on = rfid_power_on,
	.power_off = rfid_power_off,
	.transact = rfid_transact,
};
