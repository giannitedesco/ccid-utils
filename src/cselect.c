/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>

#include <stdio.h>

static int do_stuff(cci_t cci)
{
	const uint8_t *ats;
	size_t ats_len;

#if 0
	if ( !cci_wait_for_card(cci) ) {
		printf(" - wait failed\n");
		return 0;
	}
#endif
	ats = cci_power_on(cci, CHIPCARD_AUTO_VOLTAGE, &ats_len);
	if ( NULL == ats ) {
		printf(" - power on failed\n");
		return 0;
	}

	printf(" - Got %zu bytes ATS...\n", ats_len);
	hex_dump(ats, ats_len, 16);
	return 1;
}

static int found_ccid(ccidev_t dev)
{
	unsigned int i, num_slots;
	ccid_t ccid;
	cci_t cci;
	int ret = 0;

	printf("Found CCI device at %d.%d\n",
		libccid_device_bus(dev),
		libccid_device_addr(dev));
	ccid = ccid_probe(dev, "./cselect.log");
	if ( NULL == ccid )
		goto out;

	num_slots = ccid_num_slots(ccid);
	printf("CCID has %d slots\n", num_slots);
	for(i = 0; i < num_slots; i++) {
		printf(" Probing slot %d\n", i);
		cci = ccid_get_slot(ccid, i);

		if ( !do_stuff(cci) )
			continue;
	}

	num_slots = ccid_num_fields(ccid);
	printf("CCID has %d RF fields\n", num_slots);
	for(i = 0; i < num_slots; i++) {
		printf(" Probing field %d\n", i);
		cci = ccid_get_field(ccid, i);

		if ( !do_stuff(cci) )
			continue;
	}

	ret = 1;

	cci_power_off(cci);
	ccid_close(ccid);
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
		found_ccid(dev[i]);

	libccid_free_device_list(dev);

	return EXIT_SUCCESS;
}
