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

static int bop_atc(const uint8_t *ptr, size_t len, void *priv)
{
	int *ctr = priv;
	assert(1 == len);
	*ctr = *ptr;
	return 1;
}

static int atc(struct _emv *e, int online)
{
	static const struct ber_tag tags[] = {
		{ .tag = "\x9f\x13", .tag_len = 2, .op = bop_atc},
		{ .tag = "\x9f\x36", .tag_len = 2, .op = bop_atc},
	};
	const uint8_t *ptr;
	size_t len;
	int ctr = -1;

	if ( !_emv_get_data(e, 0x9f, (online) ? 0x13 : 0x36) )
		return -1;
	ptr = xfr_rx_data(e->e_xfr, &len);
	if ( NULL == ptr )
		return -1;
	if ( !ber_decode(tags, sizeof(tags)/sizeof(*tags), ptr, len, &ctr) )
		return -1;
	return ctr;
}

int emv_trm_last_online_atc(struct _emv *e)
{
	return atc(e, 1);
}

int emv_trm_atc(struct _emv *e)
{
	return atc(e, 0);
}
