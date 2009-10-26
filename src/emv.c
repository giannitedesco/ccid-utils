/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>
#include <list.h>
#include <emv.h>
#include <ber.h>
#include "emv-internal.h"

int _emv_select(emv_t e, uint8_t *name, size_t nlen)
{
	uint8_t sw2;

	assert(nlen < 0x100);
	xfr_reset(e->e_xfr);
	xfr_tx_byte(e->e_xfr, 0x00);		/* CLA */
	xfr_tx_byte(e->e_xfr, 0xa4);		/* INS: SELECT */
	xfr_tx_byte(e->e_xfr, 0x04);		/* P1: Select by name */
	xfr_tx_byte(e->e_xfr, 0);		/* P2: First/only occurance */
	xfr_tx_byte(e->e_xfr, nlen);		/* Lc: name length */
	xfr_tx_buf(e->e_xfr, name, nlen);	/* DATA: name */

	if ( !chipcard_transact(e->e_dev, e->e_xfr) )
		return 1;

	if ( xfr_rx_sw1(e->e_xfr) != 0x61 )
		return 0;
	sw2 = xfr_rx_sw2(e->e_xfr);

	xfr_reset(e->e_xfr);
	xfr_tx_byte(e->e_xfr, 0x00);		/* CLA */
	xfr_tx_byte(e->e_xfr, 0xc0);		/* INS: GET RESPONSE */
	xfr_tx_byte(e->e_xfr, 0);		/* P1 */
	xfr_tx_byte(e->e_xfr, 0);		/* P2 */
	xfr_tx_byte(e->e_xfr, sw2);		/* Le */

	if ( !chipcard_transact(e->e_dev, e->e_xfr) )
		return 0;

	if ( xfr_rx_sw1(e->e_xfr) != 0x90 )
		return 0;

	return 1;
}

int _emv_read_record(emv_t e, uint8_t sfi, uint8_t record)
{
	uint8_t sw2, p2;

	p2 = (sfi << 3) | (1 << 2);

	xfr_reset(e->e_xfr);
	xfr_tx_byte(e->e_xfr, 0x00);		/* CLA */
	xfr_tx_byte(e->e_xfr, 0xb2);		/* INS: READ RECORD */
	xfr_tx_byte(e->e_xfr, record);		/* P1: record index */
	xfr_tx_byte(e->e_xfr, p2);		/* P2 */
	xfr_tx_byte(e->e_xfr, 0);		/* Le: 0 this time around */

	if ( !chipcard_transact(e->e_dev, e->e_xfr) )
		return 0;

	if ( xfr_rx_sw1(e->e_xfr) != 0x6c )
		return 0;
	sw2 = xfr_rx_sw2(e->e_xfr);

	xfr_reset(e->e_xfr);
	xfr_tx_byte(e->e_xfr, 0x00);		/* CLA */
	xfr_tx_byte(e->e_xfr, 0xb2);		/* INS: READ RECORD */
	xfr_tx_byte(e->e_xfr, record);		/* P1: record index */
	xfr_tx_byte(e->e_xfr, p2); 		/* P2 */
	xfr_tx_byte(e->e_xfr, sw2);		/* Le: got it now */

	if ( !chipcard_transact(e->e_dev, e->e_xfr) )
		return 0;

	if ( xfr_rx_sw1(e->e_xfr) != 0x90 )
		return 0;

	return 1;
}

int _emv_get_data(emv_t e, uint8_t p1, uint8_t p2)
{
	uint8_t sw2;

	xfr_reset(e->e_xfr);
	xfr_tx_byte(e->e_xfr, 0x80);		/* CLA */
	xfr_tx_byte(e->e_xfr, 0xca);		/* INS: GET DATA*/
	xfr_tx_byte(e->e_xfr, p1);		/* P1 */
	xfr_tx_byte(e->e_xfr, p2);		/* P2 */
	xfr_tx_byte(e->e_xfr, 0);		/* Le: 0 this time around */

	if ( !chipcard_transact(e->e_dev, e->e_xfr) )
		return 0;

	if ( xfr_rx_sw1(e->e_xfr) != 0x6c )
		return 0;
	sw2 = xfr_rx_sw2(e->e_xfr);

	xfr_reset(e->e_xfr);
	xfr_tx_byte(e->e_xfr, 0x80);		/* CLA */
	xfr_tx_byte(e->e_xfr, 0xca);		/* INS: GET DATA */
	xfr_tx_byte(e->e_xfr, p1);		/* P1 */
	xfr_tx_byte(e->e_xfr, p2); 		/* P2 */
	xfr_tx_byte(e->e_xfr, sw2);		/* Le: got it now */

	if ( !chipcard_transact(e->e_dev, e->e_xfr) )
		return 0;

	if ( xfr_rx_sw1(e->e_xfr) != 0x90 )
		return 0;

	return 1;
}

int _emv_verify(emv_t e, uint8_t fmt, const uint8_t *pin, uint8_t plen)
{
	xfr_reset(e->e_xfr);
	xfr_tx_byte(e->e_xfr, 0x00);		/* CLA */
	xfr_tx_byte(e->e_xfr, 0x20);		/* INS: VERIFY */
	xfr_tx_byte(e->e_xfr, 0);		/* P1: record index */
	xfr_tx_byte(e->e_xfr, fmt);		/* P2 */
	xfr_tx_byte(e->e_xfr, plen);		/* P2 */
	xfr_tx_buf(e->e_xfr, pin, plen);

	if ( !chipcard_transact(e->e_dev, e->e_xfr) )
		return 0;

	if ( xfr_rx_sw1(e->e_xfr) != 0x90 ) {
		printf("VERIFY fail with sw1=%.2x sw2=%.2x\n",
			xfr_rx_sw1(e->e_xfr), xfr_rx_sw2(e->e_xfr));
		return 0;
	}

	return 1;
}

static void do_emv_fini(emv_t e)
{
	if ( e ) {
                struct _emv_app *a, *t;
 
		list_for_each_entry_safe(a, t, &e->e_apps, a_list) {
			list_del(&a->a_list);
			free(a);
		}
 
 		free(e->e_sda.iss_cert);
 		free(e->e_sda.iss_exp);
 		free(e->e_sda.iss_pubkey_r);
 		free(e->e_sda.ssa_data);
		RSA_free(e->e_sda.iss_pubkey);

		if ( e->e_xfr )
			xfr_free(e->e_xfr);
		free(e);
	}
}

static int bop_adfname(const uint8_t *ptr, size_t len, void *priv)
{
	struct _emv_app *a = priv;
	assert(len >= EMV_RID_LEN && len <= EMV_MAX_ADF_LEN);
	a->a_id_sz = len;
	memcpy(a->a_id, ptr, sizeof(a->a_id));
	return 1;
}

static int bop_label(const uint8_t *ptr, size_t len, void *priv)
{
	struct _emv_app *a = priv;
	snprintf(a->a_name, sizeof(a->a_name), "%.*s", len, ptr);
	return 1;
}

static int bop_pname(const uint8_t *ptr, size_t len, void *priv)
{
	struct _emv_app *a = priv;
	snprintf(a->a_pname, sizeof(a->a_pname), "%.*s", len, ptr);
	return 1;
}

static int bop_prio(const uint8_t *ptr, size_t len, void *priv)
{
	struct _emv_app *a = priv;
	assert(1U == len);
	a->a_prio = *ptr;
	return 1;
}

static int bop_dtemp(const uint8_t *ptr, size_t len, void *priv)
{
	struct _emv *e = priv;
	struct _emv_app *app;
	static const struct ber_tag tags[] = {
		{ .tag = "\x4f", .tag_len = 1, .op = bop_adfname },
		{ .tag = "\x50", .tag_len = 1, .op = bop_label },
		{ .tag = "\x87", .tag_len = 1, .op = bop_prio },
		{ .tag = "\x9f\x12", .tag_len = 2, .op = bop_pname },
	};

	app = calloc(1, sizeof(*app));
	if ( NULL == app )
		return 0;

	if ( ber_decode(tags, sizeof(tags)/sizeof(*tags), ptr, len, app) ) {
		printf(" o '%s' / '%s' application\n",
			app->a_name, app->a_pname);
		list_add_tail(&app->a_list, &e->e_apps);
		e->e_num_apps++;
		return 1;
	}else{
		free(app);
		return 0;
	}
}

static int bop_psd(const uint8_t *ptr, size_t len, void *priv)
{
	static const struct ber_tag tags[] = {
		{ .tag = "\x61", .tag_len = 1, .op = bop_dtemp },
	};
	return ber_decode(tags, sizeof(tags)/sizeof(*tags), ptr, len, priv);
}

static int add_app(emv_t e)
{
	const uint8_t *res;
	size_t len;
	static const struct ber_tag tags[] = {
		{ .tag = "\x70", .tag_len = 1, .op = bop_psd },
	};

	res = xfr_rx_data(e->e_xfr, &len);
	if ( NULL == res )
		return 0;

	return ber_decode(tags, sizeof(tags)/sizeof(*tags), res, len, e);
}

static void init_apps(emv_t e)
{
	unsigned int i;

	printf("Enumerating ICC applications:\n");
	_emv_select(e, (void *)"1PAY.SYS.DDF01", strlen("1PAY.SYS.DDF01"));

	for (i = 1; ; i++) {
		if ( !_emv_read_record(e, 1, i) )
			break;
		add_app(e);
	}
	printf("\n");
}

emv_t emv_init(chipcard_t cc)
{
	struct _emv *e;

	if ( chipcard_status(cc) != CHIPCARD_ACTIVE )
		return NULL;

	e = calloc(1, sizeof(*e));
	if ( e ) {
		e->e_dev = cc;
		INIT_LIST_HEAD(&e->e_apps);

		e->e_xfr = xfr_alloc(1024, 1204);
		if ( NULL == e->e_xfr )
			goto err;

		init_apps(e);
	}

	return e;

err:
	do_emv_fini(e);
	return NULL;
}

void emv_fini(emv_t e)
{
	do_emv_fini(e);
}
