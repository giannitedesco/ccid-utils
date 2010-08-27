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

#include <ctype.h>

uint8_t _emv_sw1(emv_t e)
{
	return xfr_rx_sw1(e->e_xfr);
}

uint8_t _emv_sw2(emv_t e)
{
	return xfr_rx_sw2(e->e_xfr);
}

#if 0
static int tag_cmp(const struct dol_tag *tag, const uint8_t *idb, size_t len)
{
	if ( tag->tag_len < len )
		return 1;
	if ( tag->tag_len > len )
		return -1;
	return memcmp(idb, tag->tag, len);
}

static const struct dol_tag *find_tag(const struct dol_tag *tags,
					unsigned int num_tags,
					const uint8_t *idb,
					size_t tag_len)
{
	while ( num_tags ) {
		unsigned int i;
		int cmp;

		i = num_tags / 2U;
		cmp = tag_cmp(tags + i, idb, tag_len);
		if ( cmp < 0 ) {
			num_tags = i;
		}else if ( cmp > 0 ) {
			tags = tags + (i + 1U);
			num_tags = num_tags - (i + 1U);
		}else
			return tags + i;
	}

	return NULL;
}
#endif

uint8_t *emv_construct_dol(emv_dol_cb_t cbfn, const uint8_t *ptr, size_t len,
				size_t *ret_len, void *priv)
{
	const uint8_t *tmp, *end;
	uint8_t *dol, *dtmp;
	size_t sz;

	end = ptr + len;

	for(sz = 0, tmp = ptr; tmp && tmp < end; ) {
		struct gber_tag tag;

		tmp = ber_tag_info(&tag, tmp, end - tmp);
		if ( NULL == tmp )
			return NULL;

		sz += tag.ber_len;
	}

	dol = dtmp = malloc(sz);
	if ( NULL == dol )
		return NULL;

	for(tmp = ptr; tmp && tmp < end; ) {
		struct gber_tag tag;

		tmp = ber_tag_info(&tag, tmp, end - tmp);
		if ( NULL == tmp ) {
			free(dol);
			return NULL;
		}

#if 0
		tag = find_tag(tags, num_tags, tmp, tag_len);

		if ( NULL == tag || NULL == tag->op ||
				!(*tag->op)(dtmp, *tmp, priv) ) {
			if ( NULL == tag ) {
				size_t i;
				printf("Unknown tag in DOL: ");
				for (i = 0; i < tag_len; i++)
					printf("%.2x", tmp[i - tag_len]);
				printf("\n");
			}
			memset(dtmp, 0, *tmp);
		}
#else
		if ( NULL == cbfn || !(*cbfn)(tag.ber_tag, dtmp,
					tag.ber_len, priv) )
			memset(dtmp, 0, *tmp);
#endif
		dtmp += tag.ber_len;
	}

	*ret_len = sz;
	return dol;
}

int _emv_pin2pb(const char *pin, emv_pb_t pb)
{
	unsigned int i;

	size_t plen;
	plen = strlen(pin);
	if ( plen < 4 || plen > 12 )
		return 0;

	memset(pb, 0xff, EMV_PIN_BLOCK_LEN);

	pb[0] = 0x20 | (plen & 0xf);
	for(i = 0; pin[i]; i++) {
		if ( !isdigit(pin[i]) )
			return 0;
		if ( i & 0x1 ) {
			pb[1 + (i >> 1)] = (pb[1 + (i >> 1)] & 0xf0) |
					((pin[i] - '0') & 0xf);
		}else{
			pb[1 + (i >> 1)] = ((pin[i] - '0') << 4) | 0xf;
		}
	}

	return 1;
}

const uint8_t *emv_generate_ac(emv_t e, uint8_t ref,
				const uint8_t *tx, uint8_t len,
				size_t *rlen)
{
	if ( !_emv_generate_ac(e, ref, tx, len) )
		return NULL;
	return xfr_rx_data(e->e_xfr, rlen);
}

static void do_emv_fini(emv_t e)
{
	if ( e ) {
		RSA_free(e->e_ca_pk);
		RSA_free(e->e_iss_pk);

		_emv_free_applist(e);

		adf_fci_free(e->e_app_fci);

		mpool_free(e->e_data);
		gang_free(e->e_files);

		free(e->e_afl);
 
		if ( e->e_xfr )
			xfr_free(e->e_xfr);

		free(e);
	}
}

emv_app_t emv_current_app(emv_t e)
{
	return e->e_app_fci;
}

emv_t emv_init(chipcard_t cc)
{
	struct _emv *e;

	if ( chipcard_status(cc) != CHIPCARD_ACTIVE )
		return NULL;

	e = calloc(1, sizeof(*e));
	if ( e ) {
		e->e_dev = cc;
		INIT_LIST_HEAD(&e->e_pse);

		e->e_xfr = xfr_alloc(1024, 1204);
		if ( NULL == e->e_xfr )
			goto err;

		e->e_data = mpool_new(sizeof(struct _emv_data), 0);
		if ( NULL == e->e_data )
			goto err;

		e->e_files = gang_new(0, 0);
		if ( NULL == e->e_files )
			goto err_free_data;
	}

	return e;

//err_free_files:
//	gang_free(e->e_files);
err_free_data:
	mpool_free(e->e_data);
err:
	do_emv_fini(e);
	return NULL;
}

void emv_fini(emv_t e)
{
	do_emv_fini(e);
}
