/*
 * This file is part of ccid-utils
 * Copyright (c) 2011 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/
#ifndef _PROTO_MFC_H
#define _PROTO_MFC_H

struct mfc_handle {
	/* stateless */
};

#define MIFARE_CL_KEYA_DEFAULT	(const uint8_t *)"\xa0\xa1\xa2\xa3\xa4\xa5"
#define MIFARE_CL_KEYB_DEFAULT	(const uint8_t *)"\xb0\xb1\xb2\xb3\xb4\xb5"

#define MIFARE_CL_KEYA_DEFAULT_INFINEON	(const uint8_t *)"\xff\xff\xff\xff\xff\xff"
#define MIFARE_CL_KEYB_DEFAULT_INFINEON MIFARE_CL_KEYA_DEFAULT_INFINEON

#define MIFARE_CL_KEY_LEN	(sizeof(MIFARE_CL_KEYA_DEFAULT)-1)

#define MIFARE_CL_PAGE_MAX	0xff
#define MIFARE_CL_PAGE_SIZE	0x10

#define RFID_CMD_MIFARE_AUTH1A	0x60
#define RFID_CMD_MIFARE_AUTH1B	0x61

#define MIFARE_CL_BLOCKS_P_SECTOR_1k	4
#define MIFARE_CL_BLOCKS_P_SECTOR_4k	16
#define MIFARE_CL_SMALL_SECTORS		32
#define MIFARE_CL_LARGE_SECTORS		4

enum rfid_proto_mfcl_opt {
	RFID_OPT_P_MFCL_SIZE	=	0x10000001,
};

extern const struct rfid_protocol rfid_protocol_mfcl;


#define MIFARE_CL_CMD_WRITE16	0xA0
#define MIFARE_CL_CMD_WRITE4	0xA2
#define MIFARE_CL_CMD_READ	0x30

#define MIFARE_CL_RESP_ACK	0x0a
#define MIFARE_CL_RESP_NAK	0x00

_private int _mfc_read(struct _cci *cci, unsigned int page,
			unsigned char *rx_data, unsigned int *rx_len);
_private int _mfc_write(struct _cci *cci, unsigned int page,
			unsigned char *tx_data, unsigned int tx_len);

extern int mfcl_sector2block(uint8_t sector);
extern int mfcl_block2sector(uint8_t block);
extern int mfcl_sector_blocks(uint8_t sector);

#endif /* PROTO_MFC_H */
