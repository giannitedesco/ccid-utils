/*
 * This file is part of ccid-utils
 * Copyright (c) 2011 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/
#ifndef _RFID_LAYER1_H
#define _RFID_LAYER1_H

#define RFID_14443A_SPEED_106K	0
#define RFID_14443A_SPEED_212K	1
#define RFID_14443A_SPEED_424K  2
#define RFID_14443A_SPEED_848K  3

#define ISO14443_FREQ_CARRIER		13560000
#define ISO14443_FREQ_SUBCARRIER	(ISO14443_FREQ_CARRIER/16)

#define RF_PARITY_ENABLE	(1<<0)
#define RF_PARITY_EVEN		(1<<1)
#define RF_TX_CRC		(1<<2)
#define RF_RX_CRC		(1<<3)
#define RF_CRYPTO1		(1<<4)
struct rf_mode {
	uint8_t tx_last_bits;
	uint8_t rx_last_bits;
	uint8_t rx_align;
	uint8_t flags;
} _packed;

#define RF_ERR_COLLISION	(1<<0)
#define RF_ERR_CRC		(1<<1)
#define RF_ERR_TIMEOUT		(1<<2)
_private int _rfid_layer1_rf_power(struct _cci *cci, unsigned int on);

_private int _rfid_layer1_set_rf_mode(struct _cci *cci,
					const struct rf_mode *rf);
_private int _rfid_layer1_get_rf_mode(struct _cci *cci,
					const struct rf_mode *rf);
_private int _rfid_layer1_get_error(struct _cci *cci, uint8_t *err);
_private int _rfid_layer1_get_coll_pos(struct _cci *cci, uint8_t *pos);
_private int _rfid_layer1_set_speed(struct _cci *cc, unsigned int i);
_private int _rfid_layer1_transact(struct _cci *cci,
					const uint8_t *tx_buf,
					uint8_t tx_len,
					uint8_t *rx_buf,
					uint8_t *rx_len,
					uint64_t timer,
					unsigned int toggle);

_private int _rfid_layer1_14443a_init(struct _cci *cci);

_private int _rfid_layer1_mfc_set_key(struct _cci *cci, const uint8_t *key);
_private int _rfid_layer1_mfc_set_key_ee(struct _cci *cci, unsigned int addr);
_private int _rfid_layer1_mfc_auth(struct _cci *cci, uint8_t cmd,
				uint32_t serial_no, uint8_t block);

_private unsigned int _rfid_layer1_carrier_freq(struct _cci *cc);
_private unsigned int _rfid_layer1_get_speeds(struct _cci *cc);
_private unsigned int _rfid_layer1_mtu(struct _cci *cc);
_private unsigned int _rfid_layer1_mru(struct _cci *cc);

struct rfid_layer1_ops {
	int (*rf_power)(struct _ccid *ccid, void *p, unsigned int on);

	int (*set_rf_mode)(struct _ccid *ccid, void *p,
				const struct rf_mode *rf);
	int (*get_rf_mode)(struct _ccid *ccid, void *p,
				const struct rf_mode *rf);
	int (*get_error)(struct _ccid *ccid, void *p, uint8_t *err);
	int (*get_coll_pos)(struct _ccid *ccid, void *p, uint8_t *pos);
	int (*set_speed)(struct _ccid *ccid, void *p, unsigned int i);
	int (*transact)(struct _ccid *ccid, void *p,
				 const uint8_t *tx_buf,
				 uint8_t tx_len,
				 uint8_t *rx_buf,
				 uint8_t *rx_len,
				 uint64_t timer,
				 unsigned int toggle);

	int (*iso14443a_init)(struct _ccid *ccid, void *p);

	int (*mfc_set_key)(struct _ccid *ccid, void *p, const uint8_t *key);
	int (*mfc_set_key_ee)(struct _ccid *ccid, void *p, unsigned int addr);
	int (*mfc_auth)(struct _ccid *ccid, void *p, uint8_t cmd,
			uint32_t serial_no, uint8_t block);

	unsigned int (*carrier_freq)(struct _ccid *ccid, void *p);
	unsigned int (*get_speeds)(struct _ccid *ccid, void *p);
	unsigned int (*mtu)(struct _ccid *ccid, void *p);
	unsigned int (*mru)(struct _ccid *ccid, void *p);

	void (*dtor)(struct _ccid *ccid, void *p);
};

_private int _rfid_init(struct _cci *cci,
			const struct rfid_layer1_ops *ops,
			void *priv);

#endif /* RFID_LAYER1_H */
