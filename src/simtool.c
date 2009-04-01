/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>
#include "sim.h"

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

static int found_cci(ccidev_t dev)
{
	unsigned int i;
	chipcard_t cc;
	cci_t cci;
	sim_t sim;
	int ret = 0;

	printf("simtool: Found a chipcard slot\n");

	cci = cci_probe(dev, NULL);
	if ( NULL == cci )
		goto out;
	
	cc = cci_get_slot(cci, 0);
	if ( NULL == cc ) {
		fprintf(stderr, "ccid: error: no slots on CCI\n");
		goto out_close;
	}

	printf("\nsimtool: wait for chipcard...\n");
	if ( !chipcard_wait_for_card(cc) )
		goto out_close;

	printf("\nsimtool: SIM attached\n");
	sim = sim_new(cc);
	if ( NULL == sim )
		goto out_close;

	printf("\nsimtool: Selecting MF\n");
	sim_select(sim, SIM_MF);

	printf("\nsimtool: Selecting EF: ICCID\n");
	sim_select(sim, SIM_EF_ICCID);
	do {
		const uint8_t *iptr;
		size_t ilen;
		iptr = sim_read_binary(sim, &ilen);
		if ( iptr )
			hex_dump(iptr, ilen, 16);

	}while(0);

	printf("\nsimtool: Selecting DF: TELECOM\n");
	sim_select(sim, SIM_DF_TELECOM);

	printf("\nsimtool: Selecting EF: SMS\n");
	sim_select(sim, SIM_EF_SMS);

	for(i = 1; i < 0xb; i++) {
		const uint8_t *sms;
		size_t sms_len;
		printf("\nsimtool: Reading SMS %u\n", i);
		sms = sim_read_record(sim, i, &sms_len);
		if ( sms ) {
			sms_dump(sms, sms_len);
		}
	}

	printf("\nsimtool: done\n");
	sim_free(sim);

	ret = 1;

out_close:
	cci_close(cci);
out:
	return ret;
}

int main(int argc, char **argv)
{
	ccidev_t dev;

	dev = ccid_find_first_device();
	if ( NULL == dev )
		return EXIT_FAILURE;

	if ( !found_cci(dev) )
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
