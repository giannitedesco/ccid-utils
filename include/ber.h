/*
 * This file is part of autober
 * Copyright (c) 2010 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#ifndef _GBER_H
#define _GBER_H

typedef uint16_t gber_tag_t;

struct gber_tag {
	uint8_t		ber_id;
	gber_tag_t	ber_tag;
	size_t		ber_len;
};

#define GBER_CLASS_UNIVERSAL	0
#define GBER_CLASS_APPLICATION	1
#define GBER_CLASS_CONTEXT	2
#define GBER_CLASS_PRIVATE	3

/* returns start of data if present or NULL if truncated */
const uint8_t *ber_decode_tag(struct gber_tag *tag,
				const uint8_t *ptr, size_t len);
const uint8_t *ber_tag_info(struct gber_tag *tag,
				const uint8_t *ptr, size_t len);
unsigned int ber_id_octet_constructed(uint8_t id);
uint8_t ber_tag_id_octet(uint16_t tag);
unsigned int ber_id_octet_class(uint8_t id);
const char * const ber_id_octet_clsname(uint8_t id);
int ber_dump(const uint8_t *ptr, size_t len);
int ber_dumpf(FILE *f, const uint8_t *ptr, size_t len);

#endif /* _GBER_H */
