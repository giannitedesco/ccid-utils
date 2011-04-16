/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/
#ifndef _CLRC632_H
#define _CLRC632_H

struct _clrc632_ops {
	int (*fifo_read)(struct _ccid *ccid, uint8_t *buf, size_t len);
	int (*fifo_write)(struct _ccid *ccid, const uint8_t *buf, size_t len);
	int (*reg_read)(struct _ccid *ccid, uint8_t reg, uint8_t *val);
	int (*reg_write)(struct _ccid *ccid, uint8_t reg, uint8_t val);
};

_private int _clrc632_init(struct _cci *cci, const struct _clrc632_ops *ops);

#endif /* _CLRC632_H */
