/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
 *
 * Interface to a chipcard rfid interface/slot.
*/

#include <ccid.h>

#include "ccid-internal.h"
#include "rfid-internal.h"
#include "iso14443a.h"

static int do_select(struct _cci *cci)
{
	struct _rfid *rf = cci->cc_priv;

	memset(&rf->rf_tag, 0, sizeof(rf->rf_tag));

	if ( !_iso14443a_anticol(cci, 0, &rf->rf_tag) )
		return 0;

	printf("Found ISO-14443-A tag: cascade level %d\n",
		rf->rf_tag.level);
	hex_dump(rf->rf_tag.uid, rf->rf_tag.uid_len, 16);

	if ( rf->rf_tag.tcl_capable )
		return _tcl_get_ats(cci, &rf->rf_tag, &rf->rf_l2.tcl);

	printf("Unsupported tag type\n");
	return 0;
}

static const uint8_t *rfid_power_on(struct _cci *cci, unsigned int voltage,
				size_t *atr_len)
{
	struct _ccid *ccid = cci->cc_parent;
	if ( !_rfid_layer1_rf_power(cci, 1) )
		return NULL;
	if ( !_rfid_layer1_14443a_init(cci) )
		return NULL;
	if ( !do_select(cci) )
		return NULL;
	if ( atr_len )
		*atr_len = ccid->cci_xfr->x_rxlen;
	return ccid->cci_xfr->x_rxbuf;
}

static int rfid_power_off(struct _cci *cci)
{
	return _rfid_layer1_rf_power(cci, 0);
}

static int rfid_transact(struct _cci *cci, struct _xfr *xfr)
{
	printf("%s: called\n", __func__);
	return 0;
}

static void rfid_dtor(struct _cci *cci)
{
	struct _rfid *rf = cci->cc_priv;
	if ( rf->rf_l1->dtor )
		rf->rf_l1->dtor(rf->rf_ccid, rf->rf_l1p);
	free(rf);
	cci->cc_priv = NULL;
}

_private const struct _cci_ops _rfid_ops = {
	.power_on = rfid_power_on,
	.power_off = rfid_power_off,
	.transact = rfid_transact,
	.dtor = rfid_dtor,
};
