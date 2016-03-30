#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <ber.h>

static void hex_dump(const uint8_t *tmp, size_t len,
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

		printf("%*c%05zx : ", depth, ' ', j);

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

#if 0
static unsigned int ber_id_octet_class(const uint8_t cls)
{
	return (cls & 0xc0) >> 6;
}
#endif

static unsigned int ber_id_octet_constructed(const uint8_t cls)
{
	return (cls & 0x20) >> 5;
}

static unsigned int ber_id_octet_tag(const uint8_t cls)
{
	return cls & 0x1f;
}

static unsigned int ber_len_form_short(const uint8_t cls)
{
	return !(cls & 0x80);
}

static unsigned int ber_len_short(const uint8_t cls)
{
	return cls & 0x7f;
}

static int tag_cmp(const struct ber_tag *tag, const uint8_t *idb, size_t len)
{
	if ( tag->tag_len < len )
		return 1;
	if ( tag->tag_len > len )
		return -1;
	return memcmp(idb, tag->tag, len);
}

static const struct ber_tag *find_tag(const struct ber_tag *tags,
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

static const uint8_t *decode_tag(const uint8_t **ptr,
					const uint8_t *end,
					size_t *tag_len)
{
	const uint8_t *tmp = *ptr;

	if ( tmp >= end ) {
		*ptr = end;
		return NULL;
	}

	if ( ber_id_octet_tag(*tmp) != 0x1f ) {
		tmp++;
	}else{
		for( tmp++; tmp < end; tmp++) {
			if ( 0 == (*tmp & 0x80) ) {
				tmp++;
				break;
			}
		}
	}

	*tag_len = tmp - *ptr;
	end = *ptr;
	*ptr = tmp;
	return end;
}

size_t ber_tag_len(const uint8_t *ptr, const uint8_t *end)
{
	size_t ret;
	if ( NULL == decode_tag(&ptr, end, &ret) )
		return 0;
	return ret;
}

const uint8_t *ber_decode_tag(const uint8_t **ptr,
					const uint8_t *end,
					size_t *tag_len)
{
	return decode_tag(ptr, end, tag_len);
}

static size_t decode_len(const uint8_t **ptr, const uint8_t *end)
{
	const uint8_t *tmp = *ptr;
	size_t ret;

	if ( ber_len_form_short(*tmp) ) {
		ret = ber_len_short(*tmp);
		tmp++;
	}else{
		size_t i, l;

		l = ber_len_short(*tmp);
		if ( l > 4 || tmp + l > end ) {
			*ptr = end;
			return 1;
		}

		tmp++;
		for(ret = i = 0; i < l; i++, tmp++) {
			ret <<= 8;
			ret |= *tmp;
		}

	}

	*ptr = tmp;
	return ret;
}

size_t ber_decode_len(const uint8_t **ptr, const uint8_t *end)
{
	return decode_len(ptr, end);
}

int ber_decode(const struct ber_tag *tags, unsigned int num_tags,
		const uint8_t *ptr, size_t len, void *priv)
{
	const uint8_t *end = ptr + len;
	unsigned int i;
	uint32_t clen;

//	printf("BER DECODE:\n");
//	hex_dump(ptr, len, 16, 0);

	for(i = 0; ptr < end; ptr += clen) {
		const uint8_t *idb;
		const struct ber_tag *tag;
		size_t tag_len;

		idb = decode_tag(&ptr, end, &tag_len);
		if ( ptr >= end )
			return 0;

		clen = decode_len(&ptr, end);
		if ( ptr + clen > end )
			return 0;

		tag = find_tag(tags, num_tags, idb, tag_len);
		if ( tag ) {
			if ( tag->op && !(*tag->op)(ptr, clen, priv) )
				return 0;
			i++;
		}else{
			size_t i;
			printf("unknown tag: ");
			for(i = 0; i < tag_len; i++)
				printf("%.2x ", idb[i]);
			printf("\n");
			hex_dump(ptr, clen, 16, 0);
		}
	}

	return i;
}
