/*
 * This file is part of ccid-utils
 * Copyright (c) 2011 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/
#ifndef _RFID_H
#define _RFID_H

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

#define RFID_LAYER2_NONE	0
#define RFID_LAYER2_ISO14443A	44431U
#define RFID_LAYER2_ISO14443B	44432U
#define RFID_LAYER2_ISO15693	15693U
#define RFID_LAYER2_ICODE1	1U
typedef unsigned long rfid_layer2_t;

#define ISO14443A_MAX_UID 10
struct rfid_tag {
	/* Layer 2 protocol selector */
	rfid_layer2_t layer2;

	/* Layer 2 state, specific to layer 2 proto */
	uint8_t state;

	uint8_t uid_len;
	uint8_t uid[ISO14443A_MAX_UID];

	/* Cascade level */
#define ISO14443A_LEVEL_NONE	0U
#define ISO14443A_LEVEL_CL1	1U
#define ISO14443A_LEVEL_CL2	2U
#define ISO14443A_LEVEL_CL3	3U
	uint8_t level;

	uint8_t tcl_capable;

	/* bitmask of supported layer 3 protocols */
#define RFID_PROTOCOL_UNKNOWN		0
#define RFID_PROTOCOL_TCL		1
#define RFID_PROTOCOL_MIFARE_UL		2
#define RFID_PROTOCOL_MIFARE_CLASSIC	3
#define RFID_PROTOCOL_ICODE_SLI		4
#define RFID_PROTOCOL_TAGIT		5
#define RFID_NUM_PROTOCOLS		6
	uint8_t proto_supported;
};

_hidden int _rfid_select(struct _cci *cci);

/* Could become a chip agnostic API */
_private int _clrc632_init(struct _cci *cc);
_private int _clrc632_rf_power(struct _cci *cci, unsigned int on);
_private int _clrc632_14443a_init(struct _cci *cci);
_private int _clrc632_set_rf_mode(struct _cci *cci, const struct rf_mode *rf);
_private int _clrc632_get_rf_mode(struct _cci *cci, const struct rf_mode *rf);
_private int _clrc632_get_error(struct _cci *cci, uint8_t *err);
_private int _clrc632_get_coll_pos(struct _cci *cci, uint8_t *pos);
_private int _clrc632_transceive(struct _cci *cci,
				 const uint8_t *tx_buf,
				 uint8_t tx_len,
				 uint8_t *rx_buf,
				 uint8_t *rx_len,
				 uint64_t timer,
				 unsigned int toggle);

#endif /* RFID_H */
