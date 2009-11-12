/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>
#include "sim-internal.h"

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
	printf(" o sms: \"%sim\"\n", out);
}

static uint8_t hi_nibble(uint8_t byte)
{
	return byte >> 4;
}

static uint8_t lo_nibble(uint8_t byte)
{
	return byte & 0xf;
}

static void sms_dump(const uint8_t *ptr, size_t len)
{
	assert(len == 176);

	switch( ptr[0] & 0x7 ) {
	case 0:
		printf(" o sms: Status: free block\n");
		return;
	case 1:
		printf(" o sms: Status: READ\n");
		break;
	case 3:
		printf(" o sms: Status: UNREAD\n");
		break;
	case 5:
		printf(" o sms: Status: SENT\n");
		break;
	case 7:
		printf(" o sms: Status: UNSENT\n");
		break;
	default:
		printf(" o sms: Status: unknown status\n");
		return;
	}

	ptr++;
	printf(" o sms: %u bytes SMSC type 0x%.2x\n", ptr[0], ptr[1]);
	printf(" o sms: +%1x%1x %1x%1x %1x%1x %1x%1x "
		"%1x%1x %1x%1x\n",
		lo_nibble(ptr[2]), hi_nibble(ptr[2]),
		lo_nibble(ptr[3]), hi_nibble(ptr[3]),
		lo_nibble(ptr[4]), hi_nibble(ptr[4]),
		lo_nibble(ptr[5]), hi_nibble(ptr[5]),
		lo_nibble(ptr[6]), hi_nibble(ptr[6]),
		lo_nibble(ptr[7]), hi_nibble(ptr[7]));
	ptr += ptr[0] + 1;
	printf(" o sms: SMS-DELIVER %u\n", *ptr);
	ptr++;
	printf(" o sms: %u byte address type 0x%.2x\n", ptr[0], ptr[1]);
	if ( ptr[1] == 0x91 ) {
		printf(" o sms: +%1x%1x %1x%1x %1x%1x %1x%1x "
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
	printf(" o sms: TP-PID %u\n", *(ptr++));
	printf(" o sms: TP-DCS %u\n", *(ptr++));
	ptr += 7;
	printf(" o sms: %u septets\n", *ptr);
	ptr++;

	decode_sms(ptr, ptr[-1]);
}
