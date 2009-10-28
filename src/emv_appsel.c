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
		printf(" o '%s' / '%s' application (prio %u)\n",
			app->a_name, app->a_pname, app->a_prio);
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

void _emv_free_applist(emv_t e)
{
	struct _emv_app *a, *t;
	list_for_each_entry_safe(a, t, &e->e_apps, a_list) {
		list_del(&a->a_list);
		free(a);
	}
}

void _emv_init_applist(emv_t e)
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

