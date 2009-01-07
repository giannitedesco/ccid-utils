/*
 * This file is part of cci-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>
#include <ccid-spec.h>

#include <stdio.h>

#include "ccid-internal.h"

void _chipcard_set_status(struct _chipcard *cc, unsigned int status)
{
	switch( status & CCID_SLOT_STATUS_MASK ) {
	case CCID_STATUS_ICC_ACTIVE:
		printf("     : ICC present and active\n");
		cc->cc_status = CHIPCARD_ACTIVE;
		break;
	case CCID_STATUS_ICC_PRESENT:
		printf("     : ICC present and inactive\n");
		cc->cc_status = CHIPCARD_PRESENT;
		break;
	case CCID_STATUS_ICC_NOT_PRESENT:
		printf("     : ICC not presnt\n");
		cc->cc_status = CHIPCARD_NOT_PRESENT;
		break;
	default:
		fprintf(stderr, "*** error: unknown chipcard status update\n");
		break;
	}
}

unsigned int chipcard_status(chipcard_t cc)
{
	return cc->cc_status;
}

unsigned int chipcard_slot_status(chipcard_t cc)
{
	const struct ccid_msg *msg;

	if ( !_PC_to_RDR_GetSlotStatus(cc->cc_parent, cc->cc_idx) )
		return CHIPCARD_CLOCK_ERR;

	msg = _RDR_to_PC(cc->cc_parent);
	if ( NULL == msg )
		return CHIPCARD_CLOCK_ERR;

	if ( msg->bMessageType != RDR_to_PC_SlotStatus || 
		msg->bSlot != cc->cc_idx ||
		!_cci_get_cmd_result(msg, NULL) )
		return CHIPCARD_CLOCK_ERR;

	return _RDR_to_PC_SlotStatus(msg);
}

int chipcard_slot_on(chipcard_t cc, unsigned int voltage)
{
	const struct ccid_msg *msg;

	if ( !_PC_to_RDR_IccPowerOn(cc->cc_parent, cc->cc_idx, voltage) )
		return 0;

	msg = _RDR_to_PC(cc->cc_parent);
	if ( NULL == msg )
		return 0;
	
	if ( msg->bMessageType != RDR_to_PC_DataBlock ||
		msg->bSlot != cc->cc_idx ||
		!_cci_get_cmd_result(msg, NULL) )
		return 0;

	return _RDR_to_PC_DataBlock(msg);
}

int chipcard_transmit(chipcard_t cc, const uint8_t *data, size_t len)
{
	const struct ccid_msg *msg;

	if ( !_PC_to_RDR_XfrBlock(cc->cc_parent, cc->cc_idx, data, len) )
		return 0;

	msg = _RDR_to_PC(cc->cc_parent);
	if ( NULL == msg )
		return 0;

	if ( msg->bMessageType != RDR_to_PC_DataBlock ||
		msg->bSlot != cc->cc_idx ||
		!_cci_get_cmd_result(msg, NULL) )
		return 0;

	return _RDR_to_PC_DataBlock(msg);
}

int chipcard_slot_off(chipcard_t cc)
{
	const struct ccid_msg *msg;

	if ( !_PC_to_RDR_IccPowerOff(cc->cc_parent, cc->cc_idx) )
		return 0;

	msg = _RDR_to_PC(cc->cc_parent);
	if ( NULL == msg )
		return 0;
	
	if ( msg->bMessageType != RDR_to_PC_SlotStatus ||
		msg->bSlot != cc->cc_idx ||
		!_cci_get_cmd_result(msg, NULL) )
		return 0;

	_RDR_to_PC_SlotStatus(msg);

	return 1;
}

void chipcard_wait_for_card(chipcard_t cc)
{
	while( cc->cc_status == CHIPCARD_NOT_PRESENT )
		_cci_wait_for_interrupt(cc->cc_parent);
}
