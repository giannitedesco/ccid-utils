/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>
#include <stdio.h>

static int emv_select(chipcard_t cc, uint8_t *name, size_t len)
{
	uint8_t buf[6 + len];
	uint8_t gbuf[] = "\x00\xc0\x00\x00\xff";
	const uint8_t *res;


	assert(len < 0x100);

	memcpy(buf, "\x00\xa4\x04\x00", 4);
	buf[4] = len;
	memcpy(buf + 5, name, len);
	buf[5 + len] = '\0';

	/* Do select and get SW1 SW2 */
	if ( !chipcard_transmit(cc, buf, 6 + len) )
		return 1;

	res = chipcard_rcvbuf(cc, &len);
	if ( NULL == res || len < 2 )
		return 0;
	
	if ( res[0] != 0x61 )
		return 0;
	
	gbuf[4] = res[1];
	if ( !chipcard_transmit(cc, gbuf, 5) )
		return 0;

	res = chipcard_rcvbuf(cc, &len);
	if ( NULL == res || 0 == len )
		return;

	ber_dump(res, len - 2, 1);
}

static int emv_read_record(chipcard_t cc, uint8_t sfi, uint8_t record)
{
	uint8_t buf[] = "\x00\xb2\xff\xff\x1d";
	const uint8_t *res;
	size_t len;

	buf[2] = record;
	buf[3] = (sfi << 3) | (1 << 2);
	buf[4] = 0;

	if ( !chipcard_transmit(cc, (void *)buf, 5) )
		return 0;

	res = chipcard_rcvbuf(cc, &len);
	if ( NULL == res || len < 2 )
		return 0;
	
	if ( res[0] != 0x6c )
		return 0;
	
	buf[4] = res[1];
	return chipcard_transmit(cc, (void *)buf, 5);
}

void do_emv_stuff(chipcard_t cc)
{
	const uint8_t *fci;
	size_t len;
	unsigned int i;

	printf("\nSELECT PAY SYS\n");
	emv_select(cc, "1PAY.SYS.DDF01", strlen("1PAY.SYS.DDF01"));

	printf("\nREAD RECORD REC 1\n");
	emv_read_record(cc, 1, 1);

	fci = chipcard_rcvbuf(cc, &len);
	if ( NULL == fci || 0 == len )
		return;
	printf("\nBER TLV DECODE OF RESPONSE\n");
	ber_dump(fci, len - 2, 1);

	printf("\nREAD RECORD REC=%u\n", 2);
	emv_read_record(cc, 1, 2);

	printf("\nREAD RECORD REC 2\n");
	emv_read_record(cc, 1, 2);
	fci = chipcard_rcvbuf(cc, &len);
	if ( NULL == fci || 0 == len )
		return;
	printf("\nBER TLV DECODE OF RESPONSE\n");
	ber_dump(fci, len - 2, 1);


	printf("\nSELECT LINK APPLICATION\n");
	emv_select(cc, "\xa0\x00\x00\x00\x03\x10\x10", 7);

	for(i = 0; i < 0xf; i++ ) {
		printf("\nREAD RECORD SFI=%u\n", i);
		emv_read_record(cc, i, 1);

		fci = chipcard_rcvbuf(cc, &len);
		if ( NULL == fci || 0 == len )
			return;
		printf("\nBER TLV DECODE OF RESPONSE\n");
		ber_dump(fci, len - 2, 1);
	}

}
