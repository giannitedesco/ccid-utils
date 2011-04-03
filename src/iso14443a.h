/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/
#ifndef _ISO14443A_H
#define _ISO14443A_H

/* ISO 14443-3, Chapter 6.3.2 */
#define ISO14443A_AC_SEL_CODE_CL1	0x93
#define ISO14443A_AC_SEL_CODE_CL2	0x92
#define ISO14443A_AC_SEL_CODE_CL3	0x97
struct iso14443a_anticol_cmd {
	uint8_t sel_code;
	uint8_t nvb;
	uint8_t uid_bits[5];
} _packed;

struct iso14443a_atqa {
	uint8_t bf_anticol:5,
		 rfu1:1,
		 uid_size:2;
	uint8_t proprietary:4,
		 rfu2:4;
} _packed;

/* Section 6.1.2 values in usec, rounded up to next usec */
#define ISO14443A_FDT_ANTICOL_LAST1     92      /* 1236 / fc = 91.15 usec */
#define ISO14443A_FDT_ANTICOL_LAST0     87      /* 1172 / fc = 86.43 usec */

_private int _iso14443a_transceive_sf(struct _cci *cci,
						uint8_t cmd,
						struct iso14443a_atqa *atqa);
_private int _iso14443ab_transceive(struct _cci *cci,
				   unsigned int frametype,
				   const uint8_t *tx_buf, unsigned int tx_len,
				   uint8_t *rx_buf, unsigned int *rx_len,
				   uint64_t timeout, unsigned int flags);
_private int _iso14443a_transceive_acf(struct _cci *cci,
					struct iso14443a_anticol_cmd *acf,
					unsigned int *bit_of_col);
_private int _iso14443a_anticol(struct _cci *cci, int wup,
				struct rfid_tag *tag);

#endif /* ISO14443A_H */
