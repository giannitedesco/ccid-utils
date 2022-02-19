/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>
#include <emv.h>

#include <string.h>

#include "ca_pubkeys.h"

struct t_app {
	const uint8_t *aid;
	size_t aid_len;
	uint8_t asi;
};

static const struct t_app apps[] = {
	{.aid = (uint8_t *)"\xa0\x00\x00\x00\x03", .aid_len = 5, .asi = 1},
	{.aid = (uint8_t *)"\xa0\x00\x00\x00\x04", .aid_len = 5, .asi = 1},
	{.aid = (uint8_t *)"\xa0\x00\x00\x00\x05", .aid_len = 5, .asi = 1},
	{.aid = (uint8_t *)"\xa0\x00\x00\x00\x25", .aid_len = 5, .asi = 1},
	{.aid = (uint8_t *)"\xa0\x00\x00\x00\x29", .aid_len = 5, .asi = 1},
	{.aid = (uint8_t *)"\xa0\x00\x00\x03\x59", .aid_len = 5, .asi = 1},
	{.aid = (uint8_t *)"\xa0\x00\x00\x01\x41", .aid_len = 5, .asi = 1},
	{.aid = (uint8_t *)"\xd4\x10\x00\x00\x01\x10\x10",
		.aid_len = 7, .asi = 1},
};

static int app_cmp(emv_app_t a, const struct t_app *b)
{
	if ( b->asi ) {
		emv_rid_t rid;
		emv_app_rid(a, rid);
		return memcmp(rid, b->aid, EMV_RID_LEN);
	}else{
		uint8_t aid[EMV_AID_LEN];
		size_t sz;
		emv_app_aid(a, aid, &sz);
		if ( sz < b->aid_len )
			return -1;
		if ( sz > b->aid_len )
			return 1;
		return memcmp(aid, b->aid, sz);
	}
}

static int app_supported(emv_app_t a)
{
	unsigned int i;

	for(i = 0; i < sizeof(apps)/sizeof(*apps); i++) {
		if ( !app_cmp(a, apps + i) )
			return 1;
	}

	do {
		uint8_t aid[EMV_AID_LEN];
		size_t sz;
		emv_app_aid(a, aid, &sz);
		hex_dump(aid, sz, 16);
	}while(0);

	return 0;
}

/* Step 0. Select application */
static int select_app(emv_t e)
{
	emv_app_t app;
	unsigned int i;

	/* Selection of apps by PSE directory */
	if ( emv_appsel_pse(e) ) {
		for(app = emv_appsel_pse_first(e); app; ) {
			if ( !app_supported(app) ) {
				printf("emvtool: unsupported PSE app: %s\n",
					emv_app_pname(app));
				emv_app_t f = app;
				app = emv_appsel_pse_next(e, app);
				emv_app_delete(f);
				continue;
			}
			printf("emvtool: PSE app: %s\n",
				emv_app_pname(app));
			app = emv_appsel_pse_next(e, app);
		}

		for(app = emv_appsel_pse_first(e); app;
			app = emv_appsel_pse_next(e, app)) {
			if ( emv_app_select_pse(e, app) ) {
				printf("emvtool: Selected PSE app: %s\n",
					emv_app_pname(app));
				return 1;
			}
		}
	}

	/* Selection of apps by terminal list */
	for(i = 0; i < sizeof(apps)/sizeof(*apps); i++) {
		if ( !emv_app_select_aid(e, apps[i].aid, apps[i].aid_len) )
			continue;

		app = emv_current_app(e);
		if ( !app_cmp(app, apps + i) ) {
			printf("emvtool: Selected AID app: %s\n",
				emv_app_pname(app));
			return 1;
		}

		while ( emv_app_select_aid_next(e, apps[i].aid,
						apps[i].aid_len) ) {
			app = emv_current_app(e);
			if ( app_cmp(app, apps + i) )
				continue;
			printf("emvtool: Selected partial AID app: %s\n",
				emv_app_pname(app));
			return 1;
		}
	}

	return 0;
}

static const struct {
	const uint8_t *mod, *exp;
	size_t mod_len, exp_len;
}ca_keys[] = {
	[5] = {.mod = mastercard1408_mod,
		.mod_len = sizeof(mastercard1408_mod),
		.exp = mastercard1408_exp,
		.exp_len = sizeof(mastercard1408_exp)},
	[7] = {.mod = visa1152_mod,
		.mod_len = sizeof(visa1152_mod),
		.exp = visa1152_exp,
		.exp_len = sizeof(visa1152_exp)},
};

static const uint8_t *get_mod(void *priv, unsigned int idx, size_t *len)
{
	printf("emvtool: Looking for key with rid index %u\n", idx);
	if ( idx >= sizeof(ca_keys)/sizeof(*ca_keys) )
		return NULL;
	*len = ca_keys[idx].mod_len;
	return ca_keys[idx].mod;
}

static const uint8_t *get_exp(void *priv, unsigned int idx, size_t *len)
{
	if ( idx >= sizeof(ca_keys)/sizeof(*ca_keys) )
		return NULL;
	*len = ca_keys[idx].exp_len;
	return ca_keys[idx].exp;
}

static int trm(emv_t e)
{
	int atc, oatc;
	atc = emv_trm_atc(e);
	oatc = emv_trm_last_online_atc(e);
	printf("emvtool: app transaction counter: %u\n", atc);
	printf("emvtool: last online transaction: %u\n", oatc);
	return 1;
}

static int do_emv_stuff(cci_t cc)
{
	emv_t e;
	emv_aip_t aip;

	e = emv_init(cc);
	if ( NULL == e ) {
		fprintf(stderr, "emv: init error\n");
		return 0;
	}

	if ( !select_app(e) ) {
		goto end;
	}

	printf("emvtool: app selected\n");


	/* Step 1. Initiate application processing */
	if ( !emv_app_init(e) ) {
		goto end;
	}

	printf("emvtool: application initiated\n");

	/* Step 2. Read application data */
	if ( !emv_read_app_data(e) ) {
		goto end;
	}

	printf("emvtool: application data retrieved\n");

	/* Step 3. Authenticate card */
	if ( !emv_app_aip(e, aip) )
		goto end;

	if ( aip[0] & EMV_AIP_SDA ) {
		if ( !emv_authenticate_static_data(e, get_mod, get_exp, NULL) )
			goto end;
		printf("emvtool: SDA: card data authenticated\n");

	}

	if ( aip[0] & EMV_AIP_DDA ) {
		if ( !emv_authenticate_dynamic(e, get_mod, get_exp, NULL) )
			goto end;
		printf("emvtool: DDA: card data authenticated\n");
	}

	/* Step 4. Authenticate cardholder */
	if ( !emv_cvm(e) )
		goto end;
#if 0
	if ( !emv_cvm_pin(e, "1337") )
		goto end;

	printf("emvtool: card holder verified\n");

	/* Step 5. Terminal risk management*/
	if ( !trm(e) )
		goto end;

	/* Step 6. Terminal/Card action analysis */
#endif

end:	/* Step 6. terminate processing */
	printf("Done: status=%s\n", emv_error_string(emv_error(e)));
	emv_fini(e);
	return 1;
}

static int found_cci(ccidev_t dev)
{
	static int count;
	char fn[128];
	cci_t cc;
	ccid_t ccid;
	int ret = 0;

	snprintf(fn, sizeof(fn), "emvtool.%u.log", count++);
	ccid = ccid_probe(dev, fn);
	if ( NULL == ccid )
		goto out;

	printf("ccid: Device %s\n", ccid_name(ccid));

	cc = ccid_get_slot(ccid, 0);
	if ( NULL == cc ) {
		fprintf(stderr, "ccid: error: no slots on CCI\n");
		goto out_close;
	}

//	if ( !cci_wait_for_card(cc) )
//		goto out_close;

	if ( !cci_power_on(cc, CHIPCARD_AUTO_VOLTAGE, NULL) )
		goto out_close;

	if ( !do_emv_stuff(cc) )
		goto out_close;

	ret = 1;

	cci_power_off(cc);
out_close:
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
		found_cci(dev[i]);

	libccid_free_device_list(dev);

	return EXIT_SUCCESS;
}
