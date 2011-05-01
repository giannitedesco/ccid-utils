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
#include "rfid_layer1.h"
#include "iso14443a.h"

#if 0
#define dprintf printf
#define dhex_dump hex_dump
#else
#define dprintf(...) do {} while(0)
#define dhex_dump(a, b, c) do {} while(0)
#endif

static int do_select(struct _cci *cci)
{
	struct _rfid *rf = cci->i_priv;
	int ret;

	memset(&rf->rf_tag, 0, sizeof(rf->rf_tag));
	memset(&rf->rf_l3p, 0, sizeof(rf->rf_l3p));
	rf->rf_l3 = NULL;

	if ( !_iso14443a_anticol(cci, 0, &rf->rf_tag) ) {
		cci->i_status = CHIPCARD_NOT_PRESENT;
		return 0;
	}

	cci->i_status = CHIPCARD_ACTIVE;

	dprintf("Found ISO-14443-A tag: cascade level %d\n",
		rf->rf_tag.level);
	dhex_dump(rf->rf_tag.uid, rf->rf_tag.uid_len, 16);

	if ( rf->rf_tag.tcl_capable ) {
		ret = _tcl_get_ats(cci, &rf->rf_tag, &rf->rf_l3p.tcl);
		if ( ret )
			rf->rf_l3 = (rfid_l3_t)_tcl_transact;
		return ret;
	}

#if 0
	printf("Unsupported tag type\n");
	if ( !_rfid_layer1_mfc_set_key(cci, MIFARE_CL_KEYA_DEFAULT) ) {
		printf("set key failed\n");
		return 0;
	}
	if ( !_rfid_layer1_mfc_auth(cci, RFID_CMD_MIFARE_AUTH1A,
					*(uint32_t *)rf->rf_tag.uid, 0) ) {
		printf("auth failed\n");
		return 0;
	}
#endif

	return 0;
}

static const uint8_t *rfid_power_on(struct _cci *cci, unsigned int voltage,
				size_t *atr_len)
{
	struct _ccid *ccid = cci->i_parent;
	if ( !_rfid_layer1_rf_power(cci, 1) )
		return NULL;
	if ( !_rfid_layer1_14443a_init(cci) )
		return NULL;
	if ( !do_select(cci) )
		return NULL;
	if ( atr_len )
		*atr_len = ccid->d_xfr->x_rxlen;
	return ccid->d_xfr->x_rxbuf;
}

static int rfid_power_off(struct _cci *cci)
{
	cci->i_status = CHIPCARD_NOT_PRESENT;
	return _rfid_layer1_rf_power(cci, 0);
}

static int rfid_transact(struct _cci *cci, struct _xfr *xfr)
{
	struct _rfid *rf = cci->i_priv;
	size_t rx_len;

	if ( NULL == rf->rf_l3 ) {
		printf("%s: called for unsupported protocol\n", __func__);
		return 0;
	}

	rx_len = xfr->x_rxmax;
	if ( !(*rf->rf_l3)(cci, &rf->rf_tag, &rf->rf_l3p,
			xfr->x_txbuf, xfr->x_txlen,
			xfr->x_rxbuf, &rx_len) )
		return 0;

	xfr->x_rxlen = rx_len;
	return 1;
}

static void rfid_dtor(struct _cci *cci)
{
	struct _rfid *rf = cci->i_priv;
	if ( rf->rf_l1->dtor )
		rf->rf_l1->dtor(rf->rf_ccid, rf->rf_l1p);
	free(rf);
	cci->i_priv = NULL;
}

_private const struct _cci_ops _rfid_ops = {
	.power_on = rfid_power_on,
	.power_off = rfid_power_off,
	.transact = rfid_transact,
	.dtor = rfid_dtor,
};
