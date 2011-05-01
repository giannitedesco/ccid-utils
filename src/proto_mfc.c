
/* Mifare Classic implementation, PCD side.
 *
 * (C) 2005-2008 by Harald Welte <laforge@gnumonks.org>
 *
 */

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 
 *  as published by the Free Software Foundation
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <ccid.h>
#include <unistd.h>

#include "ccid-internal.h"
#include "rfid.h"
#include "rfid_layer1.h"
#include "iso14443a.h"
#include "proto_mfc.h"

#define RFID_MAX_FRAMELEN	256
#define MIFARE_CL_READ_FWT	250
#define MIFARE_CL_WRITE_FWT	600

static int
mfcl_read(struct _cci *cci, unsigned int page,
	  unsigned char *rx_data, unsigned int *rx_len)
{
	unsigned char rx_buf[16];
	size_t real_rx_len = sizeof(rx_buf);
	unsigned char tx[2];

	if (page > MIFARE_CL_PAGE_MAX)
		return 0;

	tx[0] = MIFARE_CL_CMD_READ;
	tx[1] = page & 0xff;

	if ( !_iso14443ab_transceive(cci, RFID_MIFARE_FRAME, tx,
				     sizeof(tx), rx_buf, &real_rx_len,
				     MIFARE_CL_READ_FWT) )
		return 0;

	if (real_rx_len == 1 && *rx_buf == 0x04)
		return 0;

	if (real_rx_len < *rx_len)
		*rx_len = real_rx_len;

	memcpy(rx_data, rx_buf, *rx_len);

	return 1;
}

static int
mfcl_write(struct _cci *cci, unsigned int page,
	   unsigned char *tx_data, unsigned int tx_len)
{
	unsigned char tx[2];
	unsigned char rx[1];
	size_t rx_len = sizeof(rx);

	if (page > MIFARE_CL_PAGE_MAX)
		return 0;

	if (tx_len != 16)
		return 0;
	
	tx[0] = MIFARE_CL_CMD_WRITE16;
	tx[1] = page & 0xff;

	if ( !_iso14443ab_transceive(cci, RFID_MIFARE_FRAME, tx, 2, rx,
				     &rx_len, MIFARE_CL_WRITE_FWT) )
		return 0;

	if ( !_iso14443ab_transceive(cci, RFID_MIFARE_FRAME, tx_data,
				     tx_len, rx, &rx_len,
				     MIFARE_CL_WRITE_FWT) )
		return 0;

	if (rx[0] != MIFARE_CL_RESP_ACK)
		return 0;

	return 1;
}

#if 0
static int 
mfcl_getopt(struct _cci *cci, int optname, void *optval,
	    unsigned int *optlen)
{
	int ret = -EINVAL;
	uint8_t atqa[2];
	uint8_t sak;
	unsigned int atqa_size = sizeof(atqa);
	unsigned int sak_size = sizeof(sak);
	unsigned int *size = optval;

	switch (optname) {
	case RFID_OPT_PROTO_SIZE:
		if (*optlen < sizeof(*size))
			return -EINVAL;
		*optlen = sizeof(*size);
		ret = 0;
		rfid_layer2_getopt(cci, RFID_OPT_14443A_ATQA,
				   atqa, &atqa_size);
		rfid_layer2_getopt(cci, RFID_OPT_14443A_SAK,
				   &sak, &sak_size);
		if (atqa[0] == 0x04 && atqa[1] == 0x00) {
			if (sak == 0x09) {
				/* mifare mini */
				*size = 320;
			} else
				*size = 1024;
		} else if (atqa[0] == 0x02 && atqa[1] == 0x00)
			*size = 4096;
		else
			ret = -EIO;
		break;
	}

	return ret;
}
#endif

#if 0
int mfcl_set_key(struct _cci *cci, unsigned char *key)
{
	if (!cci->rh->reader->mifare_classic.setkey)
		return -ENODEV;

	return cci->rh->reader->mifare_classic.setkey(cci->rh, key);
}

int mfcl_set_key_ee(struct _cci *cci, unsigned int addr)
{
	if (!cci->rh->reader->mifare_classic.setkey_ee)
		return -ENODEV;

	return cci->rh->reader->mifare_classic.setkey_ee(cci->rh, addr);
}

int mfcl_auth(struct _cci *cci, uint8_t cmd, uint8_t block)
{
	uint32_t serno = *((uint32_t *)cci->uid);

	if (!cci->rh->reader->mifare_classic.auth)
		return -ENODEV;

	return cci->rh->reader->mifare_classic.auth(cci->rh, cmd,
						       serno, block);
}

int mfcl_block2sector(uint8_t block)
{
	if (block < MIFARE_CL_SMALL_SECTORS * MIFARE_CL_BLOCKS_P_SECTOR_1k)
		return block/MIFARE_CL_BLOCKS_P_SECTOR_1k;
	else
		return (block - MIFARE_CL_SMALL_SECTORS * MIFARE_CL_BLOCKS_P_SECTOR_1k)
					/ MIFARE_CL_BLOCKS_P_SECTOR_4k;
}

int mfcl_sector2block(uint8_t sector)
{
	if (sector < MIFARE_CL_SMALL_SECTORS)
		return sector * MIFARE_CL_BLOCKS_P_SECTOR_1k;
	else if (sector < MIFARE_CL_SMALL_SECTORS + MIFARE_CL_LARGE_SECTORS)
		return MIFARE_CL_SMALL_SECTORS * MIFARE_CL_BLOCKS_P_SECTOR_1k + 
			(sector - MIFARE_CL_SMALL_SECTORS) * MIFARE_CL_BLOCKS_P_SECTOR_4k; 
	else
		return -EINVAL;
}

int mfcl_sector_blocks(uint8_t sector)
{
	if (sector < MIFARE_CL_SMALL_SECTORS)
		return MIFARE_CL_BLOCKS_P_SECTOR_1k;
	else if (sector < MIFARE_CL_SMALL_SECTORS + MIFARE_CL_LARGE_SECTORS)
		return MIFARE_CL_BLOCKS_P_SECTOR_4k;
	else
		return -EINVAL;
}
#endif
