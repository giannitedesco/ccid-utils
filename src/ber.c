#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

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

static unsigned int ber_id_octet_class(const uint8_t cls)
{
	return (cls & 0xc0) >> 6;
}

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

void ber_dump(const uint8_t *buf, size_t len, unsigned int depth)
{
	const uint8_t *end = buf + len;
	uint32_t clen, num, i;
	uint8_t idb;

	const char * const clsname[]={
		"universal",
		"application",
		"context-specific",
		"private",
	};

again:
	if ( buf >= end )
		return;

	idb = *buf;
	num = ber_id_octet_tag(*buf);
	buf++;

	/* FIXME: if ( tag == 0x1f ) get rest of type... */
	if ( num >= 0x1f ) {
		for(num = 0, i = 0; buf < end; i++) {
			num <<= 7;
			num |= *buf & 0x7f;
			buf++;
			if ( !(*buf & 0x80) )
				break;
		}
	}

	printf("%*c.tag = %u (0x%x)\n", depth, ' ', num, num);

	if ( buf >= end )
		return;

	if ( ber_len_form_short(*buf) ) {
		clen = ber_len_short(*buf);
		buf++;
	}else{
		uint32_t l;

		l = ber_len_short(*buf);
		if ( l > 4 || buf + l > end )
			return;
		buf++;
		for(clen = i = 0; i < l; i++, buf++) {
			clen <<= 8;
			clen |= *buf;
		}

	}

	if ( buf + clen > end )
		return;

	printf("%*c.len = %u (0x%x)",
		depth, ' ', len, len);

	if ( ber_id_octet_constructed(idb) ) {
		printf(" {\n");
		ber_dump(buf, clen, depth + 2);
		printf("%*c}\n", depth, ' ');
	}else{
		printf("\n");
		hex_dump(buf, clen, 16, depth);
	}

	buf += clen;
	goto again;
}
