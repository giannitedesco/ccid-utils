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

struct db_state {
	struct _emv *e;
	struct _emv_db *db;
	struct _emv_data **rec;
	struct _emv_data **sda;
};

static int composite(emv_t e, struct _emv_data *d)
{
	const uint8_t *ptr, *end;
	struct _emv_data **elem;
	size_t num_tags;
	size_t i;

	ptr = d->d_u.du_atomic.data;
	end = ptr + d->d_u.du_atomic.len;

	for(num_tags = 0; ptr < end; num_tags++) {
		const uint8_t *tag;
		size_t tag_len;
		size_t clen;

		tag = ber_decode_tag(&ptr, end, &tag_len);
		if ( ptr >= end )
			return 0;

		clen = ber_decode_len(&ptr, end);
		if ( ptr + clen > end )
			return 0;

		ptr += clen;
	}

	elem = gang_alloc(e->e_files, num_tags * sizeof(*elem));
	if ( NULL == elem )
		return 0;

	for(ptr = d->d_u.du_atomic.data, i = 0; ptr < end; i++) {
		const uint8_t *tag;
		size_t tag_len;
		size_t clen;
		uint16_t t;

		tag = ber_decode_tag(&ptr, end, &tag_len);
		if ( ptr >= end )
			return 0;

		clen = ber_decode_len(&ptr, end);
		if ( ptr + clen > end )
			return 0;

		switch(tag_len) {
		case 1:
			t = tag[0];
			break;
		case 2:
			t = (tag[0] << 8) | tag[1];
			break;
		default:
			printf("emv: ridiculous tag len\n");
			return 0;
		}

		elem[i] = mpool_alloc(e->e_data);
		elem[i]->d_flags = EMV_DATA_BINARY;
		elem[i]->d_tag = t;
		elem[i]->d_u.du_atomic.data = ptr;
		elem[i]->d_u.du_atomic.len = clen;

		ptr += clen;
	}

	d->d_u.du_comp.nmemb = num_tags;
	d->d_u.du_comp.elem = elem;

	return 1;
}

static int decode_record(struct db_state *s, const uint8_t *ptr, size_t len)
{
	const uint8_t *end = ptr + len;
	uint8_t *tmp;
	struct _emv_data *d;

	if ( len < 2 || ptr[0] != 0x70 ) {
		printf("emv: bad application data format\n");
		return 0;
	}

	ptr++;
	len--;

	len = ber_decode_len(&ptr, end);
	if ( ptr + len > end ) {
		printf("emv: buffer overflow shiznitz\n");
		return 0;
	}

	d = mpool_alloc(s->e->e_data);
	if ( NULL == d )
		return 0;

	tmp = gang_alloc(s->e->e_files, len);
	if ( NULL == ptr )
		return 0;
	
	memcpy(tmp, ptr, len);

	hex_dump(ptr, len, 16);
	d->d_flags = EMV_DATA_BINARY;
	d->d_tag = 0x70;
	d->d_u.du_atomic.data = tmp;
	d->d_u.du_atomic.len = len;

	*s->rec = d;
	s->rec++;

	return composite(s->e, d);
}

static int read_sfi(struct db_state *s, uint8_t sfi,
			uint8_t begin, uint8_t end, uint8_t num_sda)
{
	const uint8_t *res;
	unsigned int i;
	size_t len;

	printf("Reading SFI %u\n", sfi);
	for(i = begin; i <= end; i++ ) {
		if ( !_emv_read_record(s->e, sfi, i) )
			return 0;

		res = xfr_rx_data(s->e->e_xfr, &len);
		if ( NULL == res )
			return 0;

		printf(" Record %u/%u:\n", i, end);
		if ( !decode_record(s, res, len) )
			return 0;
	}

	return 1;
}

int emv_read_app_data(struct _emv *e)
{
	struct _emv_db *db = &e->e_db;
	struct db_state s;
	struct _emv_data **pps;
	uint8_t *ptr, *end;
	unsigned int i, j;

	for(ptr = e->e_afl, end = e->e_afl + e->e_afl_len;
		ptr + 4 <= end; ptr += 4) {
		//read_sfi(e, ptr[0] >> 3, ptr[1], ptr[2]);
		printf("AFL entry: SFI %u REC %u-%u SDA %u\n",
			ptr[0] >> 3, ptr[1], ptr[2], ptr[3]);
		db->db_numsda += ptr[3];
		db->db_numrec += (ptr[2] + 1) - ptr[1];
	}
	printf("AFL: %u toplevel records, %u SDA protected records\n",
		db->db_numrec, db->db_numsda);

	pps = gang_alloc(e->e_files,
			(db->db_numrec + db->db_numsda) * sizeof(*pps));
	if ( NULL == pps )
		return 0;
	
	db->db_rec = pps;
	db->db_sda = pps + db->db_numrec;

	s.e = e;
	s.db = db;
	s.rec = db->db_rec;
	s.sda = db->db_sda;

	for(ptr = e->e_afl, end = e->e_afl + e->e_afl_len;
		ptr + 4 <= end; ptr += 4) {
		read_sfi(&s, ptr[0] >> 3, ptr[1], ptr[2], ptr[3]);
	}

#if 0
	for(i = 0; i < db->db_numrec; i++) {
		struct _emv_data **d;
		d = db->db_rec[i]->d_u.du_comp.elem;
		for(j = 0; j < db->db_rec[i]->d_u.du_comp.nmemb; j++) {
			printf(" tag %.4x\n", d[j]->d_tag);
		}
	}
#endif

	return 1;
}
