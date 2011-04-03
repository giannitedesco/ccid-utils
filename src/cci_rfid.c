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

/* TODO: Need tag selection API whereby we do collision detection, allow
 * caller to determine which tag they want to talk to and then returns
 * it's status and type. Would be stubbed out with a 'contact' type in the
 * regular smart-card case
*/

static unsigned rfid_clock_status(struct _cci *cci)
{
	return CHIPCARD_CLOCK_ERR;
}

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
	return 0;
}

static int rfid_wait_for_card(struct _cci *cci)
{
	return 1;
}

_private const struct _cci_ops _rfid_ops = {
	.clock_status = rfid_clock_status,
	.power_on = rfid_power_on,
	.power_off = rfid_power_off,
	.transact = rfid_transact,
	.wait_for_card = rfid_wait_for_card,
};
