/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>
#include "sim-internal.h"

static int do_select(struct _sim * s, uint16_t id)
{
	xfr_reset(s->s_xfr);
	xfr_tx_byte(s->s_xfr, SIM_CLA);
	xfr_tx_byte(s->s_xfr, SIM_INS_SELECT);
	xfr_tx_byte(s->s_xfr, 0); /* p1 */
	xfr_tx_byte(s->s_xfr, 0); /* p2 */
	xfr_tx_byte(s->s_xfr, 2); /* lc */
	xfr_tx_byte(s->s_xfr, (id >> 8));
	xfr_tx_byte(s->s_xfr, (id & 0xff));
	return chipcard_transact(s->s_cc, s->s_xfr);
}

static int do_get_response(struct _sim * s, uint8_t le)
{
	xfr_reset(s->s_xfr);
	xfr_tx_byte(s->s_xfr, SIM_CLA);
	xfr_tx_byte(s->s_xfr, SIM_INS_GET_RESPONSE);
	xfr_tx_byte(s->s_xfr, 0); /* p1 */
	xfr_tx_byte(s->s_xfr, 0); /* p2 */
	xfr_tx_byte(s->s_xfr, le);
	return chipcard_transact(s->s_cc, s->s_xfr);
}

int _apdu_select(struct _sim *s, uint16_t id)
{
	uint8_t sw1, sw2;

	if ( !do_select(s, id) )
		return 0;

	sw1 = xfr_rx_sw1(s->s_xfr);
	if ( sw1 != SIM_SW1_SHORT )
		return 0;

	sw2 = xfr_rx_sw2(s->s_xfr);
	if ( !do_get_response(s, sw2) )
		return 0;

	sw1 = xfr_rx_sw1(s->s_xfr);
	if ( sw1 != SIM_SW1_SUCCESS )
		return 0;

	return 1;
}

int _apdu_read_binary(struct _sim *s, uint16_t ofs, uint8_t len)
{
	xfr_reset(s->s_xfr);
	xfr_tx_byte(s->s_xfr, SIM_CLA);
	xfr_tx_byte(s->s_xfr, SIM_INS_READ_BINARY);
	xfr_tx_byte(s->s_xfr, ofs >> 8); /* p1 */
	xfr_tx_byte(s->s_xfr, ofs & 0xff); /* p2 */
	xfr_tx_byte(s->s_xfr, len);
	if ( !chipcard_transact(s->s_cc, s->s_xfr) )
		return 0;
	return ( xfr_rx_sw1(s->s_xfr) == 0x90 );
}

int _apdu_read_record(struct _sim *s, uint8_t rec, uint8_t len)
{
	xfr_reset(s->s_xfr);
	xfr_tx_byte(s->s_xfr, SIM_CLA);
	xfr_tx_byte(s->s_xfr, SIM_INS_READ_RECORD);
	xfr_tx_byte(s->s_xfr, rec);
	xfr_tx_byte(s->s_xfr, 0x4);
	xfr_tx_byte(s->s_xfr, len);
	if ( !chipcard_transact(s->s_cc, s->s_xfr) )
		return 0;
	return ( xfr_rx_sw1(s->s_xfr) == 0x90 );
}
