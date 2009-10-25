/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/
#ifndef _BER_H
#define _BER_H

struct ber_tag {
	const uint8_t *tag;
	size_t tag_len;
	int(*op)(const uint8_t *ptr, size_t len, void *priv);
};

int ber_decode(const struct ber_tag *tags, unsigned int num_tags,
		const uint8_t *ptr, size_t len, void *priv);

#endif /* _BER_H */
