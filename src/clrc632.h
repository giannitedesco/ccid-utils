/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/
#ifndef _CLRC632_H
#define _CLRC632_H

struct _clrc632_ops {
	int (*fifo_read)(struct _ccid *ccid, unsigned int field,
			 uint8_t *buf, size_t len);
	int (*fifo_write)(struct _ccid *ccid, unsigned int field,
			  const uint8_t *buf, size_t len);
	int (*reg_read)(struct _ccid *ccid, unsigned int field,
			uint8_t reg, uint8_t *val);
	int (*reg_write)(struct _ccid *ccid, unsigned int field,
			 uint8_t reg, uint8_t val);
};

_private int _clrc632_init(struct _cci *cc, const struct _clrc632_ops *asic_ops);
_private int _clrc632_rf_power(struct _cci *cci, unsigned int on);
_private int _clrc632_14443a_init(struct _cci *cci);
_private int _clrc632_set_rf_mode(struct _cci *cci, const struct rf_mode *rf);
_private int _clrc632_get_rf_mode(struct _cci *cci, const struct rf_mode *rf);
_private int _clrc632_get_error(struct _cci *cci, uint8_t *err);
_private int _clrc632_get_coll_pos(struct _cci *cci, uint8_t *pos);
_private int _clrc632_set_speed(struct _cci *cc, unsigned int i);
_private int _clrc632_transceive(struct _cci *cci,
				 const uint8_t *tx_buf,
				 uint8_t tx_len,
				 uint8_t *rx_buf,
				 uint8_t *rx_len,
				 uint64_t timer,
				 unsigned int toggle);
_private unsigned int _clrc632_carrier_freq(struct _cci *cc);
_private unsigned int _clrc632_get_speeds(struct _cci *cc);
_private unsigned int _clrc632_mtu(struct _cci *cc);
_private unsigned int _clrc632_mru(struct _cci *cc);

#endif /* _CLRC632_H */
