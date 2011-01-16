/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>
#include <sim.h>

static int found_cci(ccidev_t dev)
{
	chipcard_t cc;
	cci_t cci;
	sim_t sim;
	int ret = 0;

	printf("simtool: Found a chipcard slot\n");

	cci = cci_probe(dev, "simtool.trace");
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

	//sim_sms_save(sim, "smsdump.bin");
	//sim_sms_restore(sim, "smsdump.bin");

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
	ccidev_t *dev;
	size_t num_dev, i;

	dev = libccid_get_device_list(&num_dev);
	if ( NULL == dev )
		return EXIT_FAILURE;

	for(i = 0; i < num_dev; i++)
		found_cci(dev[i]);

	libccid_free_device_list(dev);

	return EXIT_SUCCESS;
}
