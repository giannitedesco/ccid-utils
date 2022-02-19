#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <ctype.h>
#include <gber.h>

static void hex_dumpf_r(FILE *f, const uint8_t *tmp, size_t len,
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

		fprintf(f, "%*c%05zx : ", depth, ' ', j);

		for(i = 0; i < line; i++) {
			if ( isprint(tmp[i]) ) {
				fprintf(f, "%c", tmp[i]);
			}else{
				fprintf(f, ".");
			}
		}

		for(; i < llen; i++)
			fprintf(f, " ");

		for(i=0; i < line; i++)
			fprintf(f, " %02x", tmp[i]);

		fprintf(f, "\n");
	}
	fprintf(f, "\n");
}

static int do_ber_dump(FILE *f, const uint8_t *ptr, size_t len,
			unsigned int depth)
{
	const uint8_t *end = ptr + len;

	while(ptr < end) {
		struct gber_tag tag;
		ptr = ber_decode_block(&tag, ptr, end - ptr);
		if ( NULL == ptr )
			return 0;

		fprintf(f, "%*c.tag = %x\n", depth, ' ', tag.ber_tag);
		fprintf(f, "%*c.class = %s\n", depth, ' ',
			ber_id_octet_clsname(tag.ber_id));
		fprintf(f, "%*c.constructed = %s\n", depth, ' ',
			ber_id_octet_constructed(tag.ber_id) ? "yes" : "no");

		fprintf(f, "%*c.len = %zu (0x%.2zx)\n",
			depth, ' ', tag.ber_len, tag.ber_len);

		if ( ber_id_octet_constructed(tag.ber_id) ) {
			if ( !do_ber_dump(f, ptr, tag.ber_len, depth + 1) )
				return 0;
		}else{
			hex_dumpf_r(f, ptr, tag.ber_len, 16, depth + 1);
		}

		ptr += tag.ber_len;
	}

	return 1;
}

int ber_dump(const uint8_t *ptr, size_t len)
{
	return do_ber_dump(stdout, ptr, len, 1);
}

int ber_dumpf(FILE *f, const uint8_t *ptr, size_t len)
{
	return do_ber_dump(f, ptr, len, 1);
}

const char * const ber_id_octet_clsname(uint8_t id)
{
	static const char * const clsname[]={
		"universal",
		"application",
		"context-specific",
		"private",
	};
	return clsname[(id & 0xc0) >> 6];
}

unsigned int ber_id_octet_class(uint8_t id)
{
	return (id & 0xc0) >> 6;
}

unsigned int ber_id_octet_constructed(uint8_t id)
{
	return (id & 0x20) >> 5;
}

static uint8_t ber_len_form_short(uint8_t lb)
{
	return !(lb & 0x80);
}

static uint8_t ber_len_short(uint8_t lb)
{
	return lb & ~0x80;
}

static const uint8_t *do_decode_tag(struct gber_tag *tag,
				const uint8_t *ptr, size_t len)
{
	const uint8_t *end = ptr + len;

	if ( len < 2 ) {
		printf("block too small\n");
		return NULL;
	}

	tag->ber_id = *(ptr++);
	tag->ber_tag = tag->ber_id;
	if ( (tag->ber_id & 0x1f) == 0x1f ) {
		if ( (*ptr & 0x80) ) {
			printf("bad id\n");
			return NULL;
		}
		tag->ber_tag <<= 8;
		tag->ber_tag |= *(ptr++);
		if ( ptr >= end ) {
			printf("tag too big\n");
			return NULL;
		}
	}

	if ( ber_len_form_short(*ptr) ) {
		tag->ber_len = ber_len_short(*ptr);
		ptr++;
	}else{
		unsigned int i;
		uint8_t ll;

		ll = ber_len_short(*(ptr++));
		if ( ptr + ll > end || ll > 4 ) {
			printf("tag past end\n");
			return NULL;
		}

		for(tag->ber_len = 0, i = 0; i < ll; i++, ptr++) {
			tag->ber_len <<= 8;
			tag->ber_len |= *ptr;
		}
	}

	return ptr;
}

const uint8_t *ber_tag_info(struct gber_tag *tag,
				const uint8_t *ptr, size_t len)
{
	return do_decode_tag(tag, ptr, len);
}

const uint8_t *ber_decode_block(struct gber_tag *tag,
				const uint8_t *ptr, size_t len)
{
	const uint8_t *end = ptr + len;
	ptr = do_decode_tag(tag, ptr, len);
	if ( NULL == ptr || ptr + tag->ber_len > end )
		return NULL;
	return ptr;
}
