/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/
#ifndef _ISO14443A_H
#define _ISO14443A_H

/* ISO 14443-3, Chapter 6.3.1 */
#define ISO14443A_SF_CMD_REQA 		0x26
#define ISO14443A_SF_CMD_WUPA 		0x52
#define ISO14443A_SF_CMD_OPT_TIMESLOT	0x35 /* Annex C */
/* 40 to 4f and 78 to 7f: proprietary */

/* ISO 14443-3, Chapter 6.3.2 */
struct iso14443a_anticol_cmd {
	uint8_t sel_code;
	uint8_t nvb;
	uint8_t uid_bits[5];
} _packed;
#define ISO14443A_AC_SEL_CODE_CL1	0x93
#define ISO14443A_AC_SEL_CODE_CL2	0x92
#define ISO14443A_AC_SEL_CODE_CL3	0x97

#define	ISO14443A_BITOFCOL_NONE		0xffffffff

enum rfid_frametype {
	RFID_14443A_FRAME_REGULAR,
	RFID_14443B_FRAME_REGULAR,
	RFID_MIFARE_FRAME,
	RFID_15693_FRAME,
	RFID_15693_FRAME_ICODE1,
};

//#define RFID_BIG_ENDIAN_BITFIELD
struct iso14443a_atqa {
#ifndef RFID_BIG_ENDIAN_BITFIELD
	uint8_t bf_anticol:5,
		 rfu1:1,
		 uid_size:2;
	uint8_t proprietary:4,
		 rfu2:4;
#else
	uint8_t uid_size:2,
		 rfu1:1,
		 bf_anticol:5;
	uint8_t rfu2:4,
		 proprietary:4;
#endif
} _packed;

_private int _iso14443a_select(struct _cci *cci, int wup);
_private int _clrc632_iso14443a_transceive_sf(struct _cci *cci,
						uint8_t cmd,
						struct iso14443a_atqa *atqa);
_private int _clrc632_iso14443ab_transceive(struct _cci *cci,
				   unsigned int frametype,
				   const uint8_t *tx_buf, unsigned int tx_len,
				   uint8_t *rx_buf, unsigned int *rx_len,
				   uint64_t timeout, unsigned int flags);
_private int _clrc632_iso14443a_transceive_acf(struct _cci *cci,
					struct iso14443a_anticol_cmd *acf,
					unsigned int *bit_of_col);

#endif /* ISO14443A_H */
