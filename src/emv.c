/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>
#include <list.h>
#include <emv.h>
#include "emv-internal.h"

static int emv_select(emv_t e, uint8_t *name, size_t nlen)
{
	const uint8_t *res;
	size_t len;
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

	res = xfr_rx_data(e->e_xfr, &len);
	if ( NULL == res )
		return 0;

	ber_dump(res, len, 1);
	return 1;
}

static int emv_read_record(emv_t e, uint8_t sfi, uint8_t record)
{
	const uint8_t *res;
	size_t len;
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

	res = xfr_rx_data(e->e_xfr, &len);
	if ( NULL == res )
		return 0;

	ber_dump(res, len, 1);
	return 1;
}

static void do_emv_fini(emv_t e)
{
	if ( e ) {
                struct _emv_app *a, *t;
 
 		printf("\nEMV FINI\n");
		list_for_each_entry_safe(a, t, &e->e_apps, a_list) {
			list_del(&a->a_list);
			free(a);
		}
 
		if ( e->e_xfr )
			xfr_free(e->e_xfr);
		free(e);
	}
}

static struct _emv_app *add_app(emv_t e)
{
	struct _emv_app *app;

	app = calloc(1, sizeof(*app));
	if ( app ) {
		/* recno, id_sz/id, names */
		list_add_tail(&app->a_list, &e->e_apps);
		e->e_num_apps++;
	}
	return app;
}

static void init_apps(emv_t e)
{
	unsigned int i;

	printf("\nSELECT PAY SYS\n");
	emv_select(e, (void *)"1PAY.SYS.DDF01", strlen("1PAY.SYS.DDF01"));

	for (i = 1; ; i++) {
		struct _emv_app *app;

		if ( !emv_read_record(e, 1, i) )
			break;
		app = add_app(e);
		if ( app ) {
			printf("\nRECORD %u is %.16s / %.16s\n",
				i, app->a_name, app->a_pname);
		}
	}

	printf("\n%u APPS DISCOVERED IN PAY SYS\n", e->e_num_apps);
#if 0
	printf("\nSELECT LINK APPLICATION\n");
	emv_select(e->e_dev, e->e_xfr, (void *)"\xa0\x00\x00\x00\x29\x10\x10", 7);
	for(i = 1; i < 11; i++ ) {
		printf("\nREAD RECORD SFI=%u\n", i);
		emv_read_record(e->e_dev, e->e_xfr, i, 1);
		if ( xfr_rx_sw1(e->e_xfr) == 0x6a )
			break;
	}

	printf("\nSELECT VISA APPLICATION\n");
	emv_select(e->e_dev, e->e_xfr, (void *)"\xa0\x00\x00\x00\x03\x10\x10", 7);
	for(i = 1; i < 11; i++ ) {
		printf("\nREAD RECORD SFI=%u\n", i);
		emv_read_record(e->e_dev, e->e_xfr, i, 1);
		if ( xfr_rx_sw1(e->e_xfr) == 0x6a )
			break;
	}
#endif
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
