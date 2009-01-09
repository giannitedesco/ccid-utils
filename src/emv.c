/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>
#include <stdio.h>

static int emv_select(chipcard_t cc, xfr_t xfr, uint8_t *name, size_t nlen)
{
	const uint8_t *res;
	size_t len;
	uint8_t sw2;

	assert(nlen < 0x100);
	xfr_reset(xfr);
	xfr_tx_byte(xfr, 0x00);		/* CLS */
	xfr_tx_byte(xfr, 0xa4);		/* INS: SELECT */
	xfr_tx_byte(xfr, 0x04);		/* P1: Select by name */
	xfr_tx_byte(xfr, 0);		/* P2: First/only occurance */
	xfr_tx_byte(xfr, nlen);		/* Lc: name length */
	xfr_tx_buf(xfr, name, nlen);	/* DATA: name */

	if ( !chipcard_transact(cc, xfr) )
		return 1;

	if ( xfr_rx_sw1(xfr) != 0x61 )
		return 0;
	sw2 = xfr_rx_sw2(xfr);

	xfr_reset(xfr);
	xfr_tx_byte(xfr, 0x00);		/* CLS */
	xfr_tx_byte(xfr, 0xc0);		/* INS: GET RESPONSE */
	xfr_tx_byte(xfr, 0);		/* P1 */
	xfr_tx_byte(xfr, 0);		/* P2 */
	xfr_tx_byte(xfr, sw2);		/* Le */

	if ( !chipcard_transact(cc, xfr) )
		return 0;

	if ( xfr_rx_sw1(xfr) != 0x90 )
		return 0;

	res = xfr_rx_data(xfr, &len);
	if ( NULL == res )
		return 0;

	ber_dump(res, len, 1);
	return 1;
}

static int emv_read_record(chipcard_t cc, xfr_t xfr,
				uint8_t sfi, uint8_t record)
{
	const uint8_t *res;
	size_t len;
	uint8_t sw2;

	//uint8_t buf[] = "\x00\xb2\xff\xff\x1d";
	xfr_reset(xfr);
	xfr_tx_byte(xfr, 0x00);			/* CLS */
	xfr_tx_byte(xfr, 0xb2);			/* INS: READ RECORD */
	xfr_tx_byte(xfr, record);		/* P1: record index */
	xfr_tx_byte(xfr, (sfi << 3) | (1 << 2)); /* P2 */
	xfr_tx_byte(xfr, 0);			/* Le: 0 this time around */
	if ( !chipcard_transact(cc, xfr) )
		return 0;

	if ( xfr_rx_sw1(xfr) != 0x6c )
		return 0;
	sw2 = xfr_rx_sw2(xfr);
	
	xfr_reset(xfr);
	xfr_tx_byte(xfr, 0x00);			/* CLS */
	xfr_tx_byte(xfr, 0xb2);			/* INS: READ RECORD */
	xfr_tx_byte(xfr, record);		/* P1: record index */
	xfr_tx_byte(xfr, (sfi << 3) | (1 << 2)); /* P2 */
	xfr_tx_byte(xfr, sw2);			/* Le: got it now */

	if ( !chipcard_transact(cc, xfr) )
		return 0;

	if ( xfr_rx_sw1(xfr) != 0x90 )
		return 0;

	res = xfr_rx_data(xfr, &len);
	if ( NULL == res )
		return 0;

	ber_dump(res, len, 1);
	return 1;
}

void do_emv_stuff(chipcard_t cc)
{
	xfr_t xfr;
	unsigned int i;

	xfr = xfr_alloc(1024, 1204);
	if ( NULL == xfr )
		return;

	printf("\nSELECT PAY SYS\n");
	emv_select(cc, xfr, "1PAY.SYS.DDF01", strlen("1PAY.SYS.DDF01"));


	printf("\nREAD RECORD REC 1\n");
	emv_read_record(cc, xfr, 1, 1);

	printf("\nREAD RECORD REC 2\n");
	emv_read_record(cc, xfr, 1, 2);

	printf("\nSELECT LINK APPLICATION\n");
	emv_select(cc, xfr, "\xa0\x00\x00\x00\x29\x10\x10", 7);
	for(i = 1; i < 11; i++ ) {
		printf("\nREAD RECORD SFI=%u\n", i);
		emv_read_record(cc, xfr, i, 1);
		if ( xfr_rx_sw1(xfr) == 0x6a )
			break;
	}

	printf("\nSELECT VISA APPLICATION\n");
	emv_select(cc, xfr, "\xa0\x00\x00\x00\x03\x10\x10", 7);
	for(i = 1; i < 11; i++ ) {
		printf("\nREAD RECORD SFI=%u\n", i);
		emv_read_record(cc, xfr, i, 1);
		if ( xfr_rx_sw1(xfr) == 0x6a )
			break;
	}

	xfr_free(xfr);
}
