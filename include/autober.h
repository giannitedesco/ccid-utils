/*
 * This file is part of autober
 * Copyright (c) 2010 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#ifndef _AUTOBER_H
#define _AUTOBER_H

struct autober_blob {
	uint8_t *ptr;
	size_t len;
};

#define AUTOBER_TYPE_BLOB	0
#define AUTOBER_TYPE_OCTET	1
#define AUTOBER_TYPE_INT	2 /* min = octets */
typedef unsigned int autober_type_t;

#define AUTOBER_TEMPLATE	(1<<0)
#define AUTOBER_UNION		(1<<1) /* only valid for templates */
#define AUTOBER_SEQUENCE	(1<<2) /* only valid for templates */
#define AUTOBER_OPTIONAL	(1<<3)
#define AUTOBER_CHECK_SIZE	(1<<4) /* in bytes */
struct autober_tag {
	gber_tag_t	ab_tag;
	unsigned int	ab_flags;
	size_t		ab_size[2];
	autober_type_t	ab_type;
	const char	*ab_label;
};

struct autober_constraint {
	unsigned int count;
	size_t len; /* for fixed fields only */
};

#define AUTOBER_NUM_TAGS(tags) (sizeof(tags)/sizeof(*tags))
const struct autober_tag *autober_find_tag(const struct autober_tag *tags,
					unsigned int n, gber_tag_t id);
int autober_constraints(const struct autober_tag *tags,
				struct autober_constraint *cons,
				unsigned int num_tags,
				const uint8_t *ptr, size_t len);

int autober_u8(uint8_t *out, struct gber_tag *tag, const uint8_t *ptr,
		unsigned int *cnt);
int autober_u16(uint16_t *out, struct gber_tag *tag, const uint8_t *ptr,
		unsigned int *cnt);
int autober_u32(uint32_t *out, struct gber_tag *tag, const uint8_t *ptr,
		unsigned int *cnt);
int autober_u64(uint64_t *out, struct gber_tag *tag, const uint8_t *ptr,
		unsigned int *cnt);
int autober_octet(uint8_t *out, struct gber_tag *tag, const uint8_t *ptr,
		unsigned int *cnt);
int autober_blob(struct autober_blob *out, struct gber_tag *tag,
			const uint8_t *ptr);

#endif /* _AUTOBER_H */
