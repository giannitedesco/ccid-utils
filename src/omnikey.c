/*
 * This file is part of ccid-utils
 * Copyright (c) 2010 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
 *
 * Chip card transaction buffer management.
*/

#include <ccid.h>

#include "ccid-internal.h"
#include "clrc632.h"

#define RFID_SLOT 0

static int fifo_read(struct _ccid *ccid, unsigned int field,
			uint8_t *buf, size_t len)
{
	struct _xfr *xfr = ccid->cci_xfr;

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
	if ( !_PC_to_RDR_Escape(ccid, RFID_SLOT, xfr) )
		return 0;

	if ( !_RDR_to_PC(ccid, RFID_SLOT, xfr) )
		return 0;

	memcpy(buf, xfr->x_rxbuf, len);
	return 1;
}

static int fifo_write(struct _ccid *ccid, unsigned int field,
			const uint8_t *buf, size_t len)
{
	struct _xfr *xfr = ccid->cci_xfr;

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
	if ( !_PC_to_RDR_Escape(ccid, RFID_SLOT, xfr) )
		return 0;

	if ( !_RDR_to_PC(ccid, RFID_SLOT, xfr) )
		return 0;

	return 1;
}

static int reg_read(struct _ccid *ccid, unsigned int field,
			uint8_t reg, uint8_t *val)
{
	struct _xfr *xfr = ccid->cci_xfr;

	xfr_reset(xfr);
	xfr_tx_byte(xfr, 0x20);
	xfr_tx_byte(xfr, 0x00);
	xfr_tx_byte(xfr, 0x00);
	xfr_tx_byte(xfr, 0x00);
	xfr_tx_byte(xfr, 0x01);
	xfr_tx_byte(xfr, 0x00);
	xfr_tx_byte(xfr, reg);
	if ( !_PC_to_RDR_Escape(ccid, RFID_SLOT, xfr) )
		return 0;

	if ( !_RDR_to_PC(ccid, RFID_SLOT, xfr) )
		return 0;

	if ( xfr->x_rxlen != 2 )
		return 0;

	trace(ccid, "reading reg 0x%x and got 0x%.2x (%.2x)\n",
		reg, xfr->x_rxbuf[1], xfr->x_rxbuf[0]);

	*val = xfr->x_rxbuf[1];
	return 1;
}

static int reg_write(struct _ccid *ccid, unsigned int field,
			uint8_t reg, uint8_t val)
{
	struct _xfr *xfr = ccid->cci_xfr;

	trace(ccid, "writing reg 0x%x with 0x%.2x\n", reg, val);
	xfr_reset(xfr);
	xfr_tx_byte(xfr, 0x20);
	xfr_tx_byte(xfr, 0x00);
	xfr_tx_byte(xfr, 0x01);
	xfr_tx_byte(xfr, 0x00);
	xfr_tx_byte(xfr, 0x00);
	xfr_tx_byte(xfr, 0x00);
	xfr_tx_byte(xfr, reg);
	xfr_tx_byte(xfr, val);
	if ( !_PC_to_RDR_Escape(ccid, RFID_SLOT, xfr) )
		return 0;

	if ( !_RDR_to_PC(ccid, RFID_SLOT, xfr) )
		return 0;

	return 1;
}

static const struct _clrc632_ops asic_ops = {
	.fifo_read = fifo_read,
	.fifo_write = fifo_write,
	.reg_read = reg_read,
	.reg_write = reg_write,
};

static int enable_clrc632(struct _ccid *ccid)
{
	struct _xfr *xfr = ccid->cci_xfr;

	xfr_reset(xfr);
	xfr_tx_byte(xfr, 0x1);
	if ( !_PC_to_RDR_Escape(ccid, RFID_SLOT, xfr) )
		return 0;
	if ( !_RDR_to_PC(ccid, RFID_SLOT, xfr) )
		return 0;
	return 1;
}

void _omnikey_init_prox(struct _ccid *ccid)
{
	trace(ccid, " o Omnikey proxcard RF interface detected\n");
	if ( !enable_clrc632(ccid) )
		return;

	ccid->cci_rf[ccid->cci_num_rf].cc_rc632 = &asic_ops;
	ccid->cci_rf[ccid->cci_num_rf].cc_idx = RFID_SLOT;

	if ( !_clrc632_init(ccid->cci_rf + ccid->cci_num_rf) )
		return;

	if ( !_clrc632_rf_power(ccid->cci_rf + ccid->cci_num_rf, 0) )
		return;

	ccid->cci_num_rf++;
	trace(ccid, " o CMRC632 ASIC RF interface enabled\n");
	return;
}
