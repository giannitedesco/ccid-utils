/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
 *
 * Interface to a cci interface slot.
*/

#include <ccid.h>

#include "ccid-internal.h"

/** Retrieve cached chip card status.
 * \ingroup g_cci
 *
 * @param cc \ref cci_t to query.
 *
 * Retrieve chip card status as of last transaction. Generates no traffic
 * accross physical bus to CCID.
 *
 * @return one of CHIPCARD_(ACTIVE|PRESENT|NOT_PRESENT).
 */
unsigned int cci_slot_status(cci_t cc)
{
	return cc->cc_status;
}

/** Retrieve chip card status.
 * \ingroup g_cci
 *
 * @param cc \ref cci_t to query.
 *
 * Query CCID for status of clock in relevant chip card slot.
 *
 * @return one of CHIPCARD_CLOCK_(START|STOP|STOP_L|STOP_H).
 */
unsigned int cci_clock_status(cci_t cc)
{
	struct _ccid *ccid = cc->cc_parent;

	if ( !_PC_to_RDR_GetSlotStatus(ccid, cc->cc_idx, ccid->cci_xfr) )
		return CHIPCARD_CLOCK_ERR;

	if ( !_RDR_to_PC(ccid, cc->cc_idx, ccid->cci_xfr) )
		return CHIPCARD_CLOCK_ERR;

	return _RDR_to_PC_SlotStatus(ccid, ccid->cci_xfr);
}

/** Power on a chip card slot.
 * \ingroup g_cci
 *
 * @param cc \ref cci_t to power on.
 * @param voltage Voltage selector.
 * @param atr_len Pointer to size_t to retrieve length of ATR message.
 *
 * @return NULL for failure, pointer to ATR message otherwise.
 */
const uint8_t *cci_slot_on(cci_t cc, unsigned int voltage,
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

/** Perform a chip card transaction.
 * \ingroup g_cci
 *
 * @param cc \ref cci_t for this transaction.
 * @param xfr \ref xfr_t representing the transfer buffer.
 *
 * Transactions consist of a transmit followed by a recieve.
 *
 * @return zero on failure.
 */
int cci_transact(cci_t cc, xfr_t xfr)
{
	struct _ccid *ccid = cc->cc_parent;

	if ( !_PC_to_RDR_XfrBlock(ccid, cc->cc_idx, xfr) )
		return 0;

	if ( !_RDR_to_PC(ccid, cc->cc_idx, xfr) )
		return 0;

	_RDR_to_PC_DataBlock(ccid, xfr);
	return 1;
}

/** Power off a chip card slot.
 * \ingroup g_cci
 *
 * @param cc \ref cci_t to power off.
 *
 * @return zero on failure.
 */
int cci_slot_off(cci_t cc)
{
	struct _ccid *ccid = cc->cc_parent;

	if ( !_PC_to_RDR_IccPowerOff(ccid, cc->cc_idx, ccid->cci_xfr) )
		return 0;

	if ( !_RDR_to_PC(ccid, cc->cc_idx, ccid->cci_xfr) )
		return 0;
	
	return _RDR_to_PC_SlotStatus(ccid, ccid->cci_xfr);
}

/** Wait for insertion of a chip card in to the slot.
 * \ingroup g_cci
 *
 * @param cc \ref cci_t to wait on.
 *
 * @return Always succeeds and returns 1.
 */
int cci_wait_for_card(cci_t cc)
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

/** Return pointer to CCID to which a chip card slot belongs.
 * \ingroup g_cci
 *
 * @param cc \ref cci_t to query.
 *
 * @return \ref ccid_t representing the CCID which contains the slot cc.
 */
ccid_t cci_ccid(cci_t cc)
{
	return cc->cc_parent;
}
