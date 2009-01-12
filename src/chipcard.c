/*
 * This file is part of cci-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
 *
 * Interface to a chipcard interface slot.
*/

#include <ccid.h>

#include <stdio.h>

#include "ccid-internal.h"

unsigned int chipcard_status(chipcard_t cc)
{
	return cc->cc_status;
}

unsigned int chipcard_slot_status(chipcard_t cc)
{
	struct _cci *cci = cc->cc_parent;

	if ( !_PC_to_RDR_GetSlotStatus(cci, cc->cc_idx, cci->cci_xfr) )
		return CHIPCARD_CLOCK_ERR;

	if ( !_RDR_to_PC(cci, cc->cc_idx, cci->cci_xfr) )
		return CHIPCARD_CLOCK_ERR;

	return _RDR_to_PC_SlotStatus(cci, cci->cci_xfr);
}

const uint8_t *chipcard_slot_on(chipcard_t cc, unsigned int voltage,
				size_t *atr_len)
{
	struct _cci *cci = cc->cc_parent;

	if ( !_PC_to_RDR_IccPowerOn(cci, cc->cc_idx, cci->cci_xfr, voltage) )
		return 0;

	if ( !_RDR_to_PC(cci, cc->cc_idx, cci->cci_xfr) )
		return 0;
	
	_RDR_to_PC_DataBlock(cci, cci->cci_xfr);
	if ( atr_len )
		*atr_len = cci->cci_xfr->x_rxlen;
	return cci->cci_xfr->x_rxbuf;
}

int chipcard_transact(chipcard_t cc, xfr_t xfr)
{
	struct _cci *cci = cc->cc_parent;

	if ( !_PC_to_RDR_XfrBlock(cci, cc->cc_idx, xfr) )
		return 0;

	if ( !_RDR_to_PC(cci, cc->cc_idx, xfr) )
		return 0;

	_RDR_to_PC_DataBlock(cci, xfr);
	return 1;
}

int chipcard_slot_off(chipcard_t cc)
{
	struct _cci *cci = cc->cc_parent;

	if ( !_PC_to_RDR_IccPowerOff(cci, cc->cc_idx, cci->cci_xfr) )
		return 0;

	if ( !_RDR_to_PC(cci, cc->cc_idx, cci->cci_xfr) )
		return 0;
	
	return _RDR_to_PC_SlotStatus(cci, cci->cci_xfr);
}

int chipcard_wait_for_card(chipcard_t cc)
{
	struct _cci *cci = cc->cc_parent;

	do {
		_PC_to_RDR_GetSlotStatus(cci, cc->cc_idx, cci->cci_xfr);
		_RDR_to_PC(cci, cc->cc_idx, cci->cci_xfr);
		if ( cc->cc_status != CHIPCARD_NOT_PRESENT )
			break;
		_cci_wait_for_interrupt(cci);
	} while( cc->cc_status == CHIPCARD_NOT_PRESENT );
	return 1;
}

cci_t chipcard_cci(chipcard_t cc)
{
	return cc->cc_parent;
}
