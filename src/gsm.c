/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>
#include <stdio.h>

static int gsm_select(chipcard_t cc, uint16_t id)
{
	uint8_t buf[] = "\xa0\xa4\x00\x00\x02\xff\xff";
	buf[5] = (id >> 8) & 0xff;
	buf[6] = id & 0xff;
	return chipcard_transmit(cc, buf, 7);
}

static void decode_sms(const uint8_t *inp, size_t len)
{
	uint8_t out[len + 1];
	unsigned int i;

	for(i = 0; i <= len; i++) {
		int ipos = i - i / 8;
		int offset = i % 8;

		out[i] = (inp[ipos] & (0x7F >> offset)) << offset;
		if( 0 == offset )
			continue;

		out[i] |= (inp[ipos - 1] & 
			(0x7F << (8 - offset)) & 0xFF) >> (8 - offset);
	}

	out[len] = '\0';
	printf("-sms: \"%s\"\n", out);
}

static uint8_t hi_nibble(uint8_t byte)
{
	return byte >> 4;
}

static uint8_t lo_nibble(uint8_t byte)
{
	return byte & 0xf;
}

static void gsm_read_sms(chipcard_t cc, uint8_t rec)
{
	uint8_t buf[] = "\xa0\xb2\xff\x04\xb0";
	const uint8_t *ptr, *end;
	size_t len;

	printf("\nREAD RECORD %u\n", rec);
	buf[2] = rec;
	chipcard_transmit(cc, buf, 5);

	ptr = chipcard_rcvbuf(cc, &len);
	if ( ptr == NULL || len == 0 )
		return;
	end = ptr + len;
	
	switch( ptr[0] & 0x7 ) {
	case 0:
		printf("-sms: Status: free block\n");
		return;
	case 1:
		printf("-sms: Status: READ\n");
		break;
	case 3:
		printf("-sms: Status: UNREAD\n");
		break;
	case 5:
		printf("-sms: Status: SENT\n");
		break;
	case 7:
		printf("-sms: Status: UNSENT\n");
		break;
	default:
		printf("-sms: Status: unknown status\n");
		return;
	}

	ptr++;
	printf("-sms: %u bytes SMSC type 0x%.2x\n", ptr[0], ptr[1]);
	printf("-sms: +%1x%1x %1x%1x %1x%1x %1x%1x "
		"%1x%1x %1x%1x\n",
		lo_nibble(ptr[2]), hi_nibble(ptr[2]),
		lo_nibble(ptr[3]), hi_nibble(ptr[3]),
		lo_nibble(ptr[4]), hi_nibble(ptr[4]),
		lo_nibble(ptr[5]), hi_nibble(ptr[5]),
		lo_nibble(ptr[6]), hi_nibble(ptr[6]),
		lo_nibble(ptr[7]), hi_nibble(ptr[7]));
	ptr += ptr[0] + 1;
	printf("-sms: SMS-DELIVER %u\n", *ptr);
	ptr++;
	printf("-sms: %u byte address type 0x%.2x\n", ptr[0], ptr[1]);
	if ( ptr[1] == 0x91 ) {
		printf("-sms: +%1x%1x %1x%1x %1x%1x %1x%1x "
			"%1x%1x %1x%1x\n",
			lo_nibble(ptr[2]), hi_nibble(ptr[2]),
			lo_nibble(ptr[3]), hi_nibble(ptr[3]),
			lo_nibble(ptr[4]), hi_nibble(ptr[4]),
			lo_nibble(ptr[5]), hi_nibble(ptr[5]),
			lo_nibble(ptr[6]), hi_nibble(ptr[6]),
			lo_nibble(ptr[7]), hi_nibble(ptr[7]));

		ptr += 8;
	}else{
		ptr += ptr[0] + 1;
	}
	printf("-sms: TP-PID %u\n", *(ptr++));
	printf("-sms: TP-DCS %u\n", *(ptr++));
	ptr += 7;
	printf("-sms: %u septets\n", *ptr);
	ptr++;

	decode_sms(ptr, ptr[-1]);
}

void do_gsm_stuff(chipcard_t cc)
{
	uint16_t i;

	printf("\nSELECT TELECOM/SMS\n");
	if ( !gsm_select(cc, 0x7f10) || !gsm_select(cc, 0x6f3c) )
		return;

	chipcard_transmit(cc, (void *)"\xa0\xc0\x00\x00\x0f", 5); /* GET */

	//chipcard_transmit(cc, (void *)"\xa0\xb2\x01\x04\xb0", 5);
	for(i = 1; i < 0xb; i++)
		gsm_read_sms(cc, i);
}
