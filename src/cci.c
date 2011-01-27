/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
 *
 * Unified chipcard interface (dual use, contact/contactless)
*/

#include <ccid.h>

#include "ccid-internal.h"

/** Retrieve cached chip card status.
 * \ingroup g_cci
 *
 * @param cci \ref cci_t to query.
 *
 * Retrieve chip card status as of last transaction. Generates no traffic
 * accross physical bus to CCID.
 *
 * @return one of CHIPCARD_(ACTIVE|PRESENT|NOT_PRESENT).
 */
unsigned int cci_slot_status(cci_t cci)
{
	return cci->cc_status;
}


/** Return pointer to CCID to which a chip card slot belongs.
 * \ingroup g_cci
 *
 * @param cci \ref cci_t to query.
 *
 * @return \ref ccid_t representing the CCID which contains the slot cci.
 */
ccid_t cci_ccid(cci_t cci)
{
	return cci->cc_parent;
}

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
	return (*cci->cc_ops->clock_status)(cci);
}

/** Power on a chip card slot.
 * \ingroup g_cci
 *
 * @param cci \ref cci_t to power on.
 * @param voltage Voltage selector.
 * @param atr_len Pointer to size_t to retrieve length of ATR message.
 *
 * @return NULL for failure, pointer to ATR message otherwise.
 */
const uint8_t *cci_slot_on(cci_t cci, unsigned int voltage,
				size_t *atr_len)
{
	return (*cci->cc_ops->slot_on)(cci, voltage, atr_len);
}

/** Perform a chip card transaction.
 * \ingroup g_cci
 *
 * @param cci \ref cci_t for this transaction.
 * @param xfr \ref xfr_t representing the transfer buffer.
 *
 * Transactions consist of a transmit followed by a recieve.
 *
 * @return zero on failure.
 */
int cci_transact(cci_t cci, xfr_t xfr)
{
	return (*cci->cc_ops->transact)(cci, xfr);
}

/** Power off a chip card slot.
 * \ingroup g_cci
 *
 * @param cci \ref cci_t to power off.
 *
 * @return zero on failure.
 */
int cci_slot_off(cci_t cci)
{
	return (*cci->cc_ops->slot_off)(cci);
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
	return (*cci->cc_ops->wait_for_card)(cci);
}
