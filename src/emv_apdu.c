/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>
#include <list.h>
#include <emv.h>
#include <ber.h>
#include "emv-internal.h"

static int do_sel(emv_t e, uint8_t p1, uint8_t p2,
			const uint8_t *name, size_t nlen)
{
	uint8_t sw2;

	assert(nlen < 0x100);
	xfr_reset(e->e_xfr);
	xfr_tx_byte(e->e_xfr, 0x00);		/* CLA */
	xfr_tx_byte(e->e_xfr, 0xa4);		/* INS: SELECT */
	xfr_tx_byte(e->e_xfr, p1);		/* P1: Select by name */
	xfr_tx_byte(e->e_xfr, p2);		/* P2: First/only occurance */
	xfr_tx_byte(e->e_xfr, nlen);		/* Lc: name length */
	xfr_tx_buf(e->e_xfr, name, nlen);	/* DATA: name */

	if ( !chipcard_transact(e->e_dev, e->e_xfr) )
		return 1;

	if ( xfr_rx_sw1(e->e_xfr) != 0x61 )
		return 0;
	sw2 = xfr_rx_sw2(e->e_xfr);

	xfr_reset(e->e_xfr);
	xfr_tx_byte(e->e_xfr, 0x00);		/* CLA */
	xfr_tx_byte(e->e_xfr, 0xc0);		/* INS: GET RESPONSE */
	xfr_tx_byte(e->e_xfr, 0);		/* P1 */
	xfr_tx_byte(e->e_xfr, 0);		/* P2 */
	xfr_tx_byte(e->e_xfr, sw2);		/* Le */

	if ( !chipcard_transact(e->e_dev, e->e_xfr) )
		return 0;

	if ( xfr_rx_sw1(e->e_xfr) != 0x90 )
		return 0;

	return 1;
}

int _emv_select(emv_t e, const uint8_t *name, size_t nlen)
{
	return do_sel(e, 0x04, 0, name, nlen);
}

int _emv_select_next(emv_t e, const uint8_t *name, size_t nlen)
{
	return do_sel(e, 0x04, 0x2, name, nlen);
}

int _emv_read_record(emv_t e, uint8_t sfi, uint8_t record)
{
	uint8_t sw2, p2;

	p2 = (sfi << 3) | (1 << 2);

	xfr_reset(e->e_xfr);
	xfr_tx_byte(e->e_xfr, 0x00);		/* CLA */
	xfr_tx_byte(e->e_xfr, 0xb2);		/* INS: READ RECORD */
	xfr_tx_byte(e->e_xfr, record);		/* P1: record index */
	xfr_tx_byte(e->e_xfr, p2);		/* P2 */
	xfr_tx_byte(e->e_xfr, 0);		/* Le: 0 this time around */

	if ( !chipcard_transact(e->e_dev, e->e_xfr) )
		return 0;

	if ( xfr_rx_sw1(e->e_xfr) != 0x6c )
		return 0;
	sw2 = xfr_rx_sw2(e->e_xfr);

	xfr_reset(e->e_xfr);
	xfr_tx_byte(e->e_xfr, 0x00);		/* CLA */
	xfr_tx_byte(e->e_xfr, 0xb2);		/* INS: READ RECORD */
	xfr_tx_byte(e->e_xfr, record);		/* P1: record index */
	xfr_tx_byte(e->e_xfr, p2); 		/* P2 */
	xfr_tx_byte(e->e_xfr, sw2);		/* Le: got it now */

	if ( !chipcard_transact(e->e_dev, e->e_xfr) )
		return 0;

	if ( xfr_rx_sw1(e->e_xfr) != 0x90 )
		return 0;

	return 1;
}
int _emv_get_data(emv_t e, uint8_t p1, uint8_t p2)
{
	uint8_t sw2;

	xfr_reset(e->e_xfr);
	xfr_tx_byte(e->e_xfr, 0x80);		/* CLA */
	xfr_tx_byte(e->e_xfr, 0xca);		/* INS: GET DATA*/
	xfr_tx_byte(e->e_xfr, p1);		/* P1 */
	xfr_tx_byte(e->e_xfr, p2);		/* P2 */
	xfr_tx_byte(e->e_xfr, 0);		/* Le: 0 this time around */

	if ( !chipcard_transact(e->e_dev, e->e_xfr) )
		return 0;

	if ( xfr_rx_sw1(e->e_xfr) != 0x6c )
		return 0;
	sw2 = xfr_rx_sw2(e->e_xfr);

	xfr_reset(e->e_xfr);
	xfr_tx_byte(e->e_xfr, 0x80);		/* CLA */
	xfr_tx_byte(e->e_xfr, 0xca);		/* INS: GET DATA */
	xfr_tx_byte(e->e_xfr, p1);		/* P1 */
	xfr_tx_byte(e->e_xfr, p2); 		/* P2 */
	xfr_tx_byte(e->e_xfr, sw2);		/* Le: got it now */

	if ( !chipcard_transact(e->e_dev, e->e_xfr) )
		return 0;

	if ( xfr_rx_sw1(e->e_xfr) != 0x90 )
		return 0;

	return 1;
}

int _emv_verify(emv_t e, uint8_t fmt, const uint8_t *pin, uint8_t plen)
{
	xfr_reset(e->e_xfr);
	xfr_tx_byte(e->e_xfr, 0x00);		/* CLA */
	xfr_tx_byte(e->e_xfr, 0x20);		/* INS: VERIFY */
	xfr_tx_byte(e->e_xfr, 0);		/* P1: record index */
	xfr_tx_byte(e->e_xfr, fmt);		/* P2 */
	xfr_tx_byte(e->e_xfr, plen);		/* P2 */
	xfr_tx_buf(e->e_xfr, pin, plen);

	if ( !chipcard_transact(e->e_dev, e->e_xfr) )
		return 0;

	if ( xfr_rx_sw1(e->e_xfr) != 0x90 ) {
		printf("VERIFY fail with sw1=%.2x sw2=%.2x\n",
			xfr_rx_sw1(e->e_xfr), xfr_rx_sw2(e->e_xfr));
		return 0;
	}

	return 1;
}

int _emv_get_proc_opts(emv_t e, const uint8_t *dol, uint8_t len)
{
	uint8_t sw2;

	xfr_reset(e->e_xfr);
	xfr_tx_byte(e->e_xfr, 0x80);		/* CLA */
	xfr_tx_byte(e->e_xfr, 0xa8);		/* INS: GET DATA*/
	xfr_tx_byte(e->e_xfr, 0);		/* P1 */
	xfr_tx_byte(e->e_xfr, 0);		/* P2 */
	xfr_tx_byte(e->e_xfr, len);		/* Lc */
	xfr_tx_buf(e->e_xfr, dol, len);		/* Data: PDOL */
	xfr_tx_byte(e->e_xfr, 0);		/* Le */

	if ( !chipcard_transact(e->e_dev, e->e_xfr) )
		return 0;

	if ( xfr_rx_sw1(e->e_xfr) != 0x61 )
		return 0;
	sw2 = xfr_rx_sw2(e->e_xfr);

	xfr_reset(e->e_xfr);
	xfr_tx_byte(e->e_xfr, 0x00);		/* CLA */
	xfr_tx_byte(e->e_xfr, 0xc0);		/* INS: GET RESPONSE */
	xfr_tx_byte(e->e_xfr, 0);		/* P1 */
	xfr_tx_byte(e->e_xfr, 0);		/* P2 */
	xfr_tx_byte(e->e_xfr, sw2);		/* Le */

	if ( !chipcard_transact(e->e_dev, e->e_xfr) )
		return 0;

	if ( xfr_rx_sw1(e->e_xfr) != 0x90 )
		return 0;

	return 1;
}

int _emv_generate_ac(emv_t e, uint8_t ref,
			const uint8_t *data, uint8_t len)
{
	uint8_t sw2;

	xfr_reset(e->e_xfr);
	xfr_tx_byte(e->e_xfr, 0x80);		/* CLA */
	xfr_tx_byte(e->e_xfr, 0xae);		/* INS: GENERATE AC */
	xfr_tx_byte(e->e_xfr, ref);		/* P1 */
	xfr_tx_byte(e->e_xfr, 0);		/* P2 */
	xfr_tx_byte(e->e_xfr, len);		/* Lc */
	xfr_tx_buf(e->e_xfr, data, len);	/* Data: */
	xfr_tx_byte(e->e_xfr, 0);		/* Le */

	if ( !chipcard_transact(e->e_dev, e->e_xfr) )
		return 0;

	printf("GENERATE AC SW1=%.2x SW2=%.2x\n",
		xfr_rx_sw1(e->e_xfr), xfr_rx_sw2(e->e_xfr));
	if ( xfr_rx_sw1(e->e_xfr) != 0x61 )
		return 0;
	sw2 = xfr_rx_sw2(e->e_xfr);

	return 1;
}

