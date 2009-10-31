/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>
#include <list.h>
#include <emv.h>
#include <ber.h>
#include <ctype.h>
#include "emv-internal.h"

static const struct _emv_tag unknown_soldier = {
	.t_tag = 0x00,
	.t_type = EMV_DATA_ATOMIC | EMV_DATA_BINARY,
	.t_label = "UNKNOWN",
};

static const struct _emv_tag tags[] = {
	{.t_tag = EMV_TAG_MAGSTRIP_TRACK2,
		.t_type = EMV_DATA_ATOMIC | EMV_DATA_BINARY,
		.t_label = "Magnetic Strip Track 2 Equivalent"},
	{.t_tag = EMV_TAG_PAN,
		.t_type = EMV_DATA_ATOMIC | EMV_DATA_BCD,
		.t_min = 0, .t_max = 10,
		.t_label = "Primary Account Number"},
	{.t_tag = EMV_TAG_RECORD,
		.t_type = EMV_DATA_BINARY,
		.t_label = "Application Record"},
	{.t_tag = EMV_TAG_CDOL1,
		.t_type = EMV_DATA_ATOMIC | EMV_DATA_DOL,
		.t_label = "Card Risk Management DOL1"},
	{.t_tag = EMV_TAG_CDOL2,
		.t_type = EMV_DATA_ATOMIC | EMV_DATA_DOL,
		.t_label = "Card Risk Management DOL2"},
	{.t_tag = EMV_TAG_CVM_LIST,
		.t_type = EMV_DATA_ATOMIC | EMV_DATA_BINARY,
		.t_label = "Cardholder verification Method List"},
	{.t_tag = EMV_TAG_CA_PK_INDEX,
		.t_type = EMV_DATA_ATOMIC | EMV_DATA_INT,
		.t_min = 1, .t_max = 1,
		.t_label = "CA Public Key Index"},
	{.t_tag = EMV_TAG_ISS_PK_CERT,
		.t_type = EMV_DATA_ATOMIC | EMV_DATA_BINARY,
		.t_label = "Issuer Public Key Certificate"},
	{.t_tag = EMV_TAG_ISS_PK_R,
		.t_type = EMV_DATA_ATOMIC | EMV_DATA_BINARY,
		.t_label = "Issuer Public Key Remainder"},
	{.t_tag = EMV_TAG_SSA_DATA,
		.t_type = EMV_DATA_ATOMIC | EMV_DATA_BINARY,
		.t_label = "Signed Static Authentication Data"},
	{.t_tag = EMV_TAG_CARDHOLDER_NAME,
		.t_type = EMV_DATA_ATOMIC | EMV_DATA_TEXT,
		.t_label = "Cardholder Name"},
	{.t_tag = EMV_TAG_DATE_EXP,
		.t_type = EMV_DATA_ATOMIC | EMV_DATA_DATE,
		.t_min = 3, .t_max = 3,
		.t_label = "Card Expiry Date"},
	{.t_tag = EMV_TAG_DATE_EFF,
		.t_type = EMV_DATA_ATOMIC | EMV_DATA_DATE,
		.t_min = 3, .t_max = 3,
		.t_label = "Card Effective Date"},
	{.t_tag = EMV_TAG_ISSUER_COUNTRY,
		.t_type = EMV_DATA_ATOMIC | EMV_DATA_BCD,
		.t_min = 2, .t_max = 2,
		.t_label = "Issuer Country Code"},
	{.t_tag = EMV_TAG_SERVICE_CODE,
		.t_type = EMV_DATA_ATOMIC | EMV_DATA_BCD,
		.t_min = 2, .t_max = 2,
		.t_label = "Service Code"},
	{.t_tag = EMV_TAG_PAN_SEQ,
		.t_type = EMV_DATA_ATOMIC | EMV_DATA_BCD,
		.t_min = 1, .t_max = 1,
		.t_label = "PAN sequence number"},
	{.t_tag = EMV_TAG_USAGE_CONTROL,
		.t_type = EMV_DATA_ATOMIC | EMV_DATA_BINARY,
		.t_min = 2, .t_max = 2,
		.t_label = "Application usage control"},
	{.t_tag = EMV_TAG_APP_VER,
		.t_type = EMV_DATA_ATOMIC | EMV_DATA_INT,
		.t_min = 2, .t_max = 2,
		.t_label = "Application version number"},
	{.t_tag = EMV_TAG_IAC_DEFAULT,
		.t_type = EMV_DATA_ATOMIC | EMV_DATA_BINARY,
		.t_min = 5, .t_max = 5,
		.t_label = "Issuert Action Code (Default)"},
	{.t_tag = EMV_TAG_IAC_DENY,
		.t_type = EMV_DATA_ATOMIC | EMV_DATA_BINARY,
		.t_min = 5, .t_max = 5,
		.t_label = "Issuert Action Code (Deny)"},
	{.t_tag = EMV_TAG_IAC_ONLINE,
		.t_type = EMV_DATA_ATOMIC | EMV_DATA_BINARY,
		.t_min = 5, .t_max = 5,
		.t_label = "Issuert Action Code (Online)"},
	{.t_tag = EMV_TAG_MAGSTRIP_TRACK1,
		.t_type = EMV_DATA_ATOMIC | EMV_DATA_TEXT,
		.t_label = "Magnetic Strip Track 1 Discretionary"},
	{.t_tag = EMV_TAG_ISS_PK_EXP,
		.t_type = EMV_DATA_ATOMIC | EMV_DATA_BINARY,
		.t_min = 1,
		.t_label = "Issuer Public Key Exponent"},
	{.t_tag = EMV_TAG_SDA_TAG_LIST,
		.t_type = EMV_DATA_ATOMIC | EMV_DATA_BINARY,
		.t_label = "SDA Tag List"},
};
static const unsigned int num_tags = sizeof(tags)/sizeof(*tags);

static const struct _emv_tag *find_tag(uint16_t id)
{
	const struct _emv_tag *t = tags;
	unsigned int n = num_tags;

	while ( n ) {
		unsigned int i;
		int cmp;

		i = n / 2U;
		cmp = id - t[i].t_tag;
		if ( cmp < 0 ) {
			n = i;
		}else if ( cmp > 0 ) {
			t = t + (i + 1U);
			n = n - (i + 1U);
		}else
			return t + i;
	}

	return &unknown_soldier;
}

static const struct _emv_data *find_data(struct _emv_data **db,
					unsigned int num, uint16_t id)
{
	struct _emv_data **d = db;
	unsigned int n = num_tags;

	while ( n ) {
		unsigned int i;
		int cmp;

		i = n / 2U;
		cmp = id - d[i]->d_id;
		if ( cmp < 0 ) {
			n = i;
		}else if ( cmp > 0 ) {
			d = d + (i + 1U);
			n = n - (i + 1U);
		}else
			return d[i];
	}

	return NULL;
}

const struct _emv_data *_emv_retrieve_data(emv_t e, uint16_t id)
{
	return find_data(e->e_db.db_elem, e->e_db.db_nmemb, id);
}

emv_data_t emv_retrieve_data(emv_t e, uint16_t id)
{
	return find_data(e->e_db.db_elem, e->e_db.db_nmemb, id);
}

const uint8_t *emv_data(emv_data_t d, size_t *len)
{
	*len = d->d_len;
	return d->d_data;
}

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

	ptr = d->d_data;
	end = ptr + d->d_len;

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

	for(ptr = d->d_data, i = 0; ptr < end; i++) {
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
		elem[i]->d_tag = find_tag(t);
		elem[i]->d_id = t;
		elem[i]->d_data = ptr;
		elem[i]->d_len = clen;
		if ( emv_data_composite(elem[i]) ) {
			composite(e, elem[i]);
		}else {
			d->d_elem = NULL;
			d->d_nmemb = 0;
		}

		ptr += clen;
	}

	d->d_nmemb = num_tags;
	d->d_elem = elem;

	return 1;
}

static int decode_record(struct db_state *s, const uint8_t *ptr, size_t len)
{
	const uint8_t *end = ptr + len;
	uint8_t *tmp;
	struct _emv_data *d;

	if ( len < 2 || ptr[0] != EMV_TAG_RECORD ) {
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

	d->d_tag = find_tag(EMV_TAG_RECORD);
	d->d_id = EMV_TAG_RECORD;
	d->d_data = tmp;
	d->d_len = len;

	*s->rec = d;
	s->rec++;

	return composite(s->e, d);
}

static void hex_dump_r(const uint8_t *tmp, size_t len,
			size_t llen, unsigned int depth)
{
	size_t i, j;
	size_t line;

	for(j = 0; j < len; j += line, tmp += line) {
		if ( j + llen > len ) {
			line = len - j;
		}else{
			line = llen;
		}

		printf("%*c%05x : ", depth, ' ', j);

		for(i = 0; i < line; i++) {
			if ( isprint(tmp[i]) ) {
				printf("%c", tmp[i]);
			}else{
				printf(".");
			}
		}

		for(; i < llen; i++)
			printf(" ");

		for(i=0; i < line; i++)
			printf(" %02x", tmp[i]);

		printf("\n");
	}
	printf("\n");
}

static const char *label(struct _emv_data *d)
{
	static char buf[20];

	if ( d->d_tag == &unknown_soldier ) {
		snprintf(buf, sizeof(buf),
			"Unknown tag: 0x%.4x", d->d_id);
		return buf;
	}else
		return d->d_tag->t_label;
}

static void dump_records(struct _emv_data **d, size_t num, unsigned depth)
{
	unsigned int i;

	for(i = 0; i < num; i++) {
		if ( emv_data_composite(d[i]) ) {
			printf("%*c%s {\n", depth, ' ', label(d[i]));
				
			dump_records(d[i]->d_elem,
					d[i]->d_nmemb, depth + 1);
			printf("%*c}\n\n", depth, ' ');
			continue;
		}

		printf("%*c%s\n", depth, ' ', label(d[i]));
		hex_dump_r(d[i]->d_data,
			d[i]->d_len, 16, depth);
	}
}

static int cmp(const void *A, const void *B)
{
	const struct _emv_data * const *a = A, * const *b = B;
	return (*a)->d_id - (*b)->d_id;
}

static void count_elements(struct _emv_data **d, size_t num,
				unsigned int *cnt)
{
	unsigned int i;

	for(i = 0; i < num; i++) {
		if ( emv_data_composite(d[i]) ) {
			(*cnt)++;
			qsort(d[i]->d_elem, d[i]->d_nmemb,
				sizeof(d[i]->d_elem), cmp);

			/* take advantage of oppertunity to sort elements
			 * for rapid searching of children */
			count_elements(d[i]->d_elem,
					d[i]->d_nmemb, cnt);
			continue;
		}
		(*cnt)++;
	}
}

static void add_elements(struct _emv_data **d, size_t num,
				struct _emv_data ***db)
{
	unsigned int i;

	for(i = 0; i < num; i++) {
		if ( emv_data_composite(d[i]) ) {
			(**db) = d[i];
			(*db)++;
			add_elements(d[i]->d_elem,
					d[i]->d_nmemb, db);
			continue;
		}
		(**db) = d[i];
		(*db)++;
	}
}

static int read_sfi(struct db_state *s, uint8_t sfi,
			uint8_t begin, uint8_t end, uint8_t num_sda)
{
	const uint8_t *res;
	unsigned int i;
	size_t len;

//	printf("Reading SFI %u\n", sfi);
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
	unsigned int i;

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

	for(i = 0; i < db->db_numrec; i++) {
		count_elements(db->db_rec[i]->d_elem,
			db->db_rec[i]->d_nmemb,
			&db->db_nmemb);
	}

	printf("%u Data Elements in total\n", db->db_nmemb);
	pps = db->db_elem = gang_alloc(e->e_files,
					db->db_nmemb * sizeof(*db->db_elem));
	if ( NULL == pps )
		return 0;

	for(i = 0; i < db->db_numrec; i++) {
		add_elements(db->db_rec[i]->d_elem,
			db->db_rec[i]->d_nmemb,
			&pps);
	}
	qsort(db->db_elem, db->db_nmemb, sizeof(*db->db_elem), cmp);

	//&dump_records(db->db_rec, db->db_numrec, 1);
	//for(i = 0; i < db->db_nmemb; i++)
	//	printf("%u. %s\n", i, label(db->db_elem[i]));
	return 1;
}
