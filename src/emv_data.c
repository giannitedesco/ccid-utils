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

static int bop_rec(const uint8_t *ptr, size_t len, void *priv)
{
	hex_dump(ptr, len, 16);
	return 1;
}

static int read_sfi(emv_t e, uint8_t sfi, uint8_t begin, uint8_t end)
{
	static const struct ber_tag tags[] = {
		{ .tag = "\x70", .tag_len = 1, .op = bop_rec},
	};
	const uint8_t *res;
	size_t len;
	unsigned int i;

	printf("Reading SFI %u records %u - %u\n", sfi, begin, end);
	for(i = begin; i <= end; i++ ) {
		if ( !_emv_read_record(e, sfi, i) )
			break;

		res = xfr_rx_data(e->e_xfr, &len);
		if ( NULL == res )
			break;

		ber_decode(tags, sizeof(tags)/sizeof(*tags), res, len, e);
	}

	return 1;
}

int _emv_read_app_data(struct _emv *e)
{
	uint8_t *ptr, *end;

	for(ptr = e->e_afl, end = e->e_afl + e->e_afl_len;
		ptr + 4 <= end; ptr += 4) {
		read_sfi(e, ptr[0] >> 3, ptr[1], ptr[2]);
	}

	return 1;
}
