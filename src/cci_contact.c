/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
 *
 * Interface to a chipcard contact interface/slot.
*/

#include <ccid.h>

#include "ccid-internal.h"


/** Retrieve chip card status.
 * \ingroup g_cci
 *
 * @param cci \ref cci_t to query.
 *
 * Query CCID for status of clock in relevant chip card slot.
 *
 * @return one of CHIPCARD_CLOCK_(START|STOP|STOP_L|STOP_H).
 */
unsigned int cci_clock_status(cci_t cci)
{
	struct _ccid *ccid = cci->cc_parent;

	if ( cci->cc_ops != &_contact_ops )
		return CHIPCARD_CLOCK_ERR;

	if ( !_PC_to_RDR_GetSlotStatus(ccid, cci->cc_idx, ccid->cci_xfr) )
		return CHIPCARD_CLOCK_ERR;

	if ( !_RDR_to_PC(ccid, cci->cc_idx, ccid->cci_xfr) )
		return CHIPCARD_CLOCK_ERR;

	return _RDR_to_PC_SlotStatus(ccid, ccid->cci_xfr);
}

static const uint8_t *contact_power_on(struct _cci *cci, unsigned int voltage,
				size_t *atr_len)
{
	struct _ccid *ccid = cci->cc_parent;

	if ( cci->cc_ops != &_contact_ops )
		return 0;

	if ( !_PC_to_RDR_IccPowerOn(ccid, cci->cc_idx, ccid->cci_xfr, voltage) )
		return 0;

	if ( !_RDR_to_PC(ccid, cci->cc_idx, ccid->cci_xfr) )
		return 0;
	
	_RDR_to_PC_DataBlock(ccid, ccid->cci_xfr);
	if ( atr_len )
		*atr_len = ccid->cci_xfr->x_rxlen;
	return ccid->cci_xfr->x_rxbuf;
}

static int contact_power_off(struct _cci *cci)
{
	struct _ccid *ccid = cci->cc_parent;

	if ( !_PC_to_RDR_IccPowerOff(ccid, cci->cc_idx, ccid->cci_xfr) )
		return 0;

	if ( !_RDR_to_PC(ccid, cci->cc_idx, ccid->cci_xfr) )
		return 0;
	
	return _RDR_to_PC_SlotStatus(ccid, ccid->cci_xfr);
}

static int contact_transact(struct _cci *cci, struct _xfr *xfr)
{
	struct _ccid *ccid = cci->cc_parent;

	if ( !_PC_to_RDR_XfrBlock(ccid, cci->cc_idx, xfr) )
		return 0;

	if ( !_RDR_to_PC(ccid, cci->cc_idx, xfr) )
		return 0;

	_RDR_to_PC_DataBlock(ccid, xfr);
	return 1;
}

/** Wait for insertion of a chip card in to the slot.
 * \ingroup g_cci
 *
 * @param cci \ref cci_t to wait on.
 *
 * @return Always succeeds and returns 1.
 */
int cci_wait_for_card(cci_t cci)
{
	struct _ccid *ccid = cci->cc_parent;

	do {
		_PC_to_RDR_GetSlotStatus(ccid, cci->cc_idx, ccid->cci_xfr);
		_RDR_to_PC(ccid, cci->cc_idx, ccid->cci_xfr);
		if ( cci->cc_status != CHIPCARD_NOT_PRESENT )
			break;
		_cci_wait_for_interrupt(ccid);
	} while( cci->cc_status == CHIPCARD_NOT_PRESENT );
	return 1;
}

_private const struct _cci_ops _contact_ops = {
	.power_on = contact_power_on,
	.power_off = contact_power_off,
	.transact = contact_transact,
};
