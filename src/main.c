/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>

#include <stdio.h>

void do_gsm_stuff(chipcard_t cc);
void do_emv_stuff(chipcard_t cc);

static int found_cci(ccidev_t dev)
{
	chipcard_t cc;
	cci_t cci;
	int ret = 0;

	cci = cci_probe(dev, "./scsh.log");
	if ( NULL == cci )
		goto out;
	
	cc = cci_get_slot(cci, 0);
	if ( NULL == cc ) {
		fprintf(stderr, "ccid: error: no slots on CCI\n");
		goto out_close;
	}

	printf("\nWAIT FOR CHIP CARD\n");
	if ( !chipcard_wait_for_card(cc) )
		goto out_close;

	printf("\nPOWER ON SLOT\n");
	if ( !chipcard_slot_on(cc, CHIPCARD_AUTO_VOLTAGE, NULL) )
		goto out_close;

#if 0
	do_gsm_stuff(cc);
#else
	do_emv_stuff(cc);
#endif

	printf("\nPOWER OFF SLOT\n");
	chipcard_slot_off(cc);

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
