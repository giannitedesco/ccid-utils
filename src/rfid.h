/*
 * This file is part of ccid-utils
 * Copyright (c) 2011 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/
#ifndef _RFID_H
#define _RFID_H

#define RFID_LAYER2_NONE	0
#define RFID_LAYER2_ISO14443A	44431U
#define RFID_LAYER2_ISO14443B	44432U
#define RFID_LAYER2_ISO15693	15693U
#define RFID_LAYER2_ICODE1	1U
typedef unsigned long rfid_layer2_t;

/* ==================[ RFID Tag State ]================== */
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
};

/* ==================[ API ]================== */
struct _rfid *rfid_t;

#endif /* RFID_H */
