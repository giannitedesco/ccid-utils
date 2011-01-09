/*
 * This file is part of cci-utils
 * Copyright (c) 2010 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
 *
 * Chip card transaction buffer management.
*/

#include <ccid.h>

#include "ccid-internal.h"

#define RFID_SLOT 0

static int fifo_read(struct _cci *cci, unsigned int field,
			uint8_t *buf, size_t len)
{
	struct _xfr *xfr = cci->cci_xfr;

	assert(len < 0x100);

	xfr_reset(xfr);
	xfr_tx_byte(xfr, 0x20);
	xfr_tx_byte(xfr, 0x00);
	xfr_tx_byte(xfr, 0x00);
	xfr_tx_byte(xfr, 0x00);
	xfr_tx_byte(xfr, len);
	xfr_tx_byte(xfr, 0x00);
	xfr_tx_byte(xfr, 0x02);
	xfr_tx_buf(xfr, buf, len);
	if ( !_PC_to_RDR_Escape(cci, RFID_SLOT, xfr) )
		return 0;

	if ( !_RDR_to_PC(cci, RFID_SLOT, xfr) )
		return 0;

	memcpy(buf, xfr->x_rxbuf, len);
	return 1;
}

static int fifo_write(struct _cci *cci, unsigned int field,
			const uint8_t *buf, size_t len)
{
	struct _xfr *xfr = cci->cci_xfr;

	assert(len < 0x100);

	xfr_reset(xfr);
	xfr_tx_byte(xfr, 0x20);
	xfr_tx_byte(xfr, 0x00);
	xfr_tx_byte(xfr, len);
	xfr_tx_byte(xfr, 0x00);
	xfr_tx_byte(xfr, 0x00);
	xfr_tx_byte(xfr, 0x03);
	xfr_tx_byte(xfr, 0x02);
	xfr_tx_buf(xfr, buf, len);
	if ( !_PC_to_RDR_Escape(cci, RFID_SLOT, xfr) )
		return 0;

	if ( !_RDR_to_PC(cci, RFID_SLOT, xfr) )
		return 0;

	return 1;
}

static int reg_read(struct _cci *cci, unsigned int field,
			unsigned int reg, uint8_t *val)
{
	struct _xfr *xfr = cci->cci_xfr;

	xfr_reset(xfr);
	xfr_tx_byte(xfr, 0x20);
	xfr_tx_byte(xfr, 0x00);
	xfr_tx_byte(xfr, 0x00);
	xfr_tx_byte(xfr, 0x00);
	xfr_tx_byte(xfr, 0x01);
	xfr_tx_byte(xfr, 0x00);
	xfr_tx_byte(xfr, reg);
	if ( !_PC_to_RDR_Escape(cci, RFID_SLOT, xfr) )
		return 0;

	if ( !_RDR_to_PC(cci, RFID_SLOT, xfr) )
		return 0;

	if ( xfr->x_rxlen != 2 )
		return 0;

	trace(cci, "reading reg 0x%x and got 0x%.2x (%.2x)\n",
		reg, xfr->x_rxbuf[1], xfr->x_rxbuf[0]);

	*val = xfr->x_rxbuf[1];
	return 1;
}

static int reg_write(struct _cci *cci, unsigned int field,
			unsigned int reg, uint8_t val)
{
	struct _xfr *xfr = cci->cci_xfr;

	trace(cci, "writing reg 0x%x with 0x%.2x\n", reg, val);
	xfr_reset(xfr);
	xfr_tx_byte(xfr, 0x20);
	xfr_tx_byte(xfr, 0x00);
	xfr_tx_byte(xfr, 0x01);
	xfr_tx_byte(xfr, 0x00);
	xfr_tx_byte(xfr, 0x00);
	xfr_tx_byte(xfr, 0x00);
	xfr_tx_byte(xfr, reg);
	xfr_tx_byte(xfr, val);
	if ( !_PC_to_RDR_Escape(cci, RFID_SLOT, xfr) )
		return 0;

	if ( !_RDR_to_PC(cci, RFID_SLOT, xfr) )
		return 0;

	return 1;
}

static const struct _clrc632_ops asic_ops = {
	.fifo_read = fifo_read,
	.fifo_write = fifo_write,
	.reg_read = reg_read,
	.reg_write = reg_write,
};

static int enable_clrc632(struct _cci *cci)
{
	struct _xfr *xfr = cci->cci_xfr;

	xfr_reset(xfr);
	xfr_tx_byte(xfr, 0x1);
	if ( !_PC_to_RDR_Escape(cci, RFID_SLOT, xfr) )
		return 0;
	if ( !_RDR_to_PC(cci, RFID_SLOT, xfr) )
		return 0;
	return 1;
}

void _omnikey_init_prox(struct _cci *cci)
{
	trace(cci, " o Omnikey proxcard RF interface detected\n");
	if ( !enable_clrc632(cci) )
		return;

	cci->cci_rf[cci->cci_num_rf].cc_rc632 = &asic_ops;
	cci->cci_rf[cci->cci_num_rf].cc_idx = RFID_SLOT;
	_clrc632_init(cci->cci_rf + cci->cci_num_rf);
	cci->cci_num_rf++;
	trace(cci, " o CMRC632 ASIC RF interface enabled\n");
}
