/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>
#include <emv.h>

#include <stdio.h>

static const emv_rid_t rids[] = {
	"\xa0\x00\x00\x00\x03",
	"\xa0\x00\x00\x00\x29",
};

static int do_emv_stuff(chipcard_t cc)
{
	emv_t e;

	e = emv_init(cc);
	if ( NULL == e ) {
		fprintf(stderr, "emv: init error\n");
		return 0;
	}

	if ( emv_appsel_pse(e) ) {
		emv_app_t app;

		for(app = emv_appsel_pse_first(e); app;
			app = emv_appsel_pse_next(e, app)) {
			printf("emvtool: PSE app: %s\n",
				emv_app_pname(app));
		}

	}else{
	}

#if 0
	if ( emv_visa_select(emv) ) {
		if ( emv_visa_offline_auth(emv) ) {
			printf("CARD VERIFIED\n");
			if ( emv_visa_cvm_pin(emv, "1337") ) {
				printf("CARDHOLDER VERIFIED\n");
			}
		}
	}
#endif

	emv_fini(e);
	return 1;
}

static int found_cci(ccidev_t dev)
{
	chipcard_t cc;
	cci_t cci;
	int ret = 0;

	cci = cci_probe(dev, "./emvtool.log");
	if ( NULL == cci )
		goto out;
	
	cc = cci_get_slot(cci, 0);
	if ( NULL == cc ) {
		fprintf(stderr, "ccid: error: no slots on CCI\n");
		goto out_close;
	}

	if ( !chipcard_wait_for_card(cc) )
		goto out_close;

	if ( !chipcard_slot_on(cc, CHIPCARD_AUTO_VOLTAGE, NULL) )
		goto out_close;

	if ( !do_emv_stuff(cc) )
		goto out_close;

	ret = 1;

	chipcard_slot_off(cc);
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
