/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
 *
 * Interface to a chipcard contact interface/slot.
*/

#include <ccid.h>

#include "ccid-internal.h"

static unsigned contact_clock_status(struct _cci *cc)
{
	struct _ccid *ccid = cc->cc_parent;

	if ( !_PC_to_RDR_GetSlotStatus(ccid, cc->cc_idx, ccid->cci_xfr) )
		return CHIPCARD_CLOCK_ERR;

	if ( !_RDR_to_PC(ccid, cc->cc_idx, ccid->cci_xfr) )
		return CHIPCARD_CLOCK_ERR;

	return _RDR_to_PC_SlotStatus(ccid, ccid->cci_xfr);
}

static const uint8_t *contact_slot_on(struct _cci *cc, unsigned int voltage,
				size_t *atr_len)
{
	struct _ccid *ccid = cc->cc_parent;

	if ( !_PC_to_RDR_IccPowerOn(ccid, cc->cc_idx, ccid->cci_xfr, voltage) )
		return 0;

	if ( !_RDR_to_PC(ccid, cc->cc_idx, ccid->cci_xfr) )
		return 0;
	
	_RDR_to_PC_DataBlock(ccid, ccid->cci_xfr);
	if ( atr_len )
		*atr_len = ccid->cci_xfr->x_rxlen;
	return ccid->cci_xfr->x_rxbuf;
}

static int contact_slot_off(struct _cci *cc)
{
	struct _ccid *ccid = cc->cc_parent;

	if ( !_PC_to_RDR_IccPowerOff(ccid, cc->cc_idx, ccid->cci_xfr) )
		return 0;

	if ( !_RDR_to_PC(ccid, cc->cc_idx, ccid->cci_xfr) )
		return 0;
	
	return _RDR_to_PC_SlotStatus(ccid, ccid->cci_xfr);
}

static int contact_transact(struct _cci *cc, struct _xfr *xfr)
{
	struct _ccid *ccid = cc->cc_parent;

	if ( !_PC_to_RDR_XfrBlock(ccid, cc->cc_idx, xfr) )
		return 0;

	if ( !_RDR_to_PC(ccid, cc->cc_idx, xfr) )
		return 0;

	_RDR_to_PC_DataBlock(ccid, xfr);
	return 1;
}

static int contact_wait_for_card(struct _cci *cc)
{
	struct _ccid *ccid = cc->cc_parent;

	do {
		_PC_to_RDR_GetSlotStatus(ccid, cc->cc_idx, ccid->cci_xfr);
		_RDR_to_PC(ccid, cc->cc_idx, ccid->cci_xfr);
		if ( cc->cc_status != CHIPCARD_NOT_PRESENT )
			break;
		_cci_wait_for_interrupt(ccid);
	} while( cc->cc_status == CHIPCARD_NOT_PRESENT );
	return 1;
}

_hidden const struct _cci_ops _contact_ops = {
	.clock_status = contact_clock_status,
	.slot_on = contact_slot_on,
	.slot_off = contact_slot_off,
	.transact = contact_transact,
	.wait_for_card = contact_wait_for_card,
};
