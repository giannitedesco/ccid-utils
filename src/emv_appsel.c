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
	assert(len >= EMV_RID_LEN && len <= EMV_AID_LEN);
	a->a_id_sz = len;
	memcpy(a->a_id, ptr, sizeof(a->a_id));
	return 1;
}

static int bop_label(const uint8_t *ptr, size_t len, void *priv)
{
	struct _emv_app *a = priv;
	snprintf(a->a_name, sizeof(a->a_name), "%.*s", (int)len, ptr);
	return 1;
}

static int bop_pname(const uint8_t *ptr, size_t len, void *priv)
{
	struct _emv_app *a = priv;
	snprintf(a->a_pname, sizeof(a->a_pname), "%.*s", (int)len, ptr);
	return 1;
}

static int bop_pdol(const uint8_t *ptr, size_t len, void *priv)
{
	struct _emv_app *a = priv;
	hex_dump(ptr, len, 16);
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
		list_add_tail(&app->a_list, &e->e_apps);
		e->e_num_apps++;
		return 1;
	}else{
		_emv_error(e, EMV_ERR_DATA_ELEMENT_NOT_FOUND);
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

	if ( !ber_decode(tags, sizeof(tags)/sizeof(*tags), res, len, e) ) {
		_emv_error(e, EMV_ERR_DATA_ELEMENT_NOT_FOUND);
		return 0;
	}else
		return 1;
}

void _emv_free_applist(emv_t e)
{
	struct _emv_app *a, *t;
	list_for_each_entry_safe(a, t, &e->e_apps, a_list) {
		list_del(&a->a_list);
		free(a);
	}
}

void emv_app_rid(emv_app_t a, emv_rid_t ret)
{
	memcpy(ret, a->a_id, EMV_RID_LEN);
}

void emv_app_aid(emv_app_t a, uint8_t *ret, size_t *len)
{
	memcpy(ret, a->a_id, a->a_id_sz);
	*len = a->a_id_sz;
}

const char *emv_app_label(emv_app_t a)
{
	return a->a_name;
}

const char *emv_app_pname(emv_app_t a)
{
	if ( '\0' != *a->a_pname )
		return a->a_pname;
	return a->a_name;
}

uint8_t emv_app_prio(emv_app_t a)
{
	return a->a_prio & 0x7f;
}

int emv_app_confirm(emv_app_t a)
{
	return a->a_prio >> 7;
}

int emv_appsel_pse(emv_t e)
{
	static const char * const pse = "1PAY.SYS.DDF01";
	struct _emv_app *a, *tmp;
	unsigned int i;

	if ( !_emv_select(e, (uint8_t *)pse, strlen(pse)) )
		return 0;

	list_for_each_entry_safe(a, tmp, &e->e_apps, a_list) {
		list_del(&a->a_list);
		free(a);
	}

	for (i = 1; ; i++) {
		if ( !_emv_read_record(e, 1, i) )
			break;
		add_app(e);
	}

	/* TODO: Sort by priority */

	_emv_success(e);
	return 1;
}

emv_app_t emv_appsel_pse_first(emv_t e)
{
	if ( list_empty(&e->e_apps) )
		return NULL;
	return list_entry(e->e_apps.next, struct _emv_app, a_list);
}

emv_app_t emv_appsel_pse_next(emv_t e, emv_app_t app)
{
	if ( app->a_list.next == &e->e_apps )
		return NULL;
	return list_entry(app->a_list.next, struct _emv_app, a_list);
}

_public void emv_app_delete(emv_app_t a)
{
	list_del(&a->a_list);
	free(a);
}

static int bop_fci2(const uint8_t *ptr, size_t len, void *priv)
{
	static const struct ber_tag tags[] = {
		{ .tag = "\x50", .tag_len = 1, .op = bop_label},
		{ .tag = "\x87", .tag_len = 1, .op = bop_prio},
		{ .tag = "\x5f\x2d", .tag_len = 2, .op = NULL},
		{ .tag = "\x9f\x11", .tag_len = 2, .op = NULL},
		{ .tag = "\x9f\x12", .tag_len = 2, .op = bop_pname},
		{ .tag = "\x9f\x38", .tag_len = 2, .op = bop_pdol},
		{ .tag = "\xbf\x0c", .tag_len = 2, .op = NULL},
		/* FIXME: retrieve optional PDOL if present */
	};
	return ber_decode(tags, sizeof(tags)/sizeof(*tags), ptr, len, priv);
}

static int bop_fci(const uint8_t *ptr, size_t len, void *priv)
{
	static const struct ber_tag tags[] = {
		{ .tag = "\x84", .tag_len = 1, .op = bop_adfname},
		{ .tag = "\xa5", .tag_len = 1, .op = bop_fci2},
	};
	return ber_decode(tags, sizeof(tags)/sizeof(*tags), ptr, len, priv);
}

static int set_app(emv_t e)
{
	static const struct ber_tag tags[] = {
		{ .tag = "\x6f", .tag_len = 1, .op = bop_fci},
	};
	struct _emv_app *cur;
	const uint8_t *fci;
	size_t len;

	_emv_auth_reset(e);

	fci = xfr_rx_data(e->e_xfr, &len);
	if ( NULL == fci )
		return 0;

	cur = calloc(1, sizeof(*cur));
	if ( NULL == cur )
		return 0;

	if ( !ber_decode(tags, BER_NUM_TAGS(tags), fci, len, cur) ) {
		free(cur);
		return 0;
	}

	e->e_app = cur;
	return 1;
}

int emv_app_select_pse(emv_t e, emv_app_t a)
{
	if ( !_emv_select(e, a->a_id, a->a_id_sz) )
		return 0;
	return set_app(e);
}

int emv_app_select_aid(emv_t e, const uint8_t *aid, size_t aid_len)
{
	if ( !_emv_select(e, aid, aid_len) )
		return 0;
	return set_app(e);
}

int emv_app_select_aid_next(emv_t e, const uint8_t *aid, size_t aid_len)
{
	if ( !_emv_select_next(e, aid, aid_len) )
		return 0;
	return set_app(e);
}
