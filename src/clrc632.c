/*
 * This file is part of cci-utils
 * Copyright (c) 2010 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
 *
 * CLRC632 RFID ASIC driver
*/

#include <ccid.h>
#include <unistd.h>

#include "ccid-internal.h"
#include "clrc632.h"
#include "iso14443a.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x)/sizeof(*x))
#endif

struct reg_file {
	uint8_t reg;
	uint8_t val;
};

static int reg_read(struct _cci *cci, uint8_t reg, uint8_t *val)
{
	return (*cci->cc_rc632->reg_read)(cci->cc_parent, cci->cc_idx, reg, val);
}

static int reg_write(struct _cci *cci, uint8_t reg, uint8_t val)
{
	return (*cci->cc_rc632->reg_write)(cci->cc_parent, cci->cc_idx, reg, val);
}

static int asic_clear_bits(struct _cci *cci, uint8_t reg, uint8_t bits)
{
	uint8_t val;

	if ( !reg_read(cci, reg, &val) )
		return 0;
	
	if ( (val & bits) == 0 )
		return 1;
	
	return reg_write(cci, reg, val & ~bits);
}

static int asic_set_bits(struct _cci *cci, uint8_t reg, uint8_t bits)
{
	uint8_t val;

	if ( !reg_read(cci, reg, &val) )
		return 0;
	
	if ( (val & bits) == bits )
		return 1;
	
	return reg_write(cci, reg, val | bits);
}

static int reg_write_batch(struct _cci *cci, const struct reg_file *r, 
				unsigned int num)
{
	unsigned int i;
	for(i = 0; i < num; i++) {
		if ( !reg_write(cci, r[i].reg, r[i].val) )
			return 0;
	}
	return 1;
}

static int asic_power(struct _cci *cci, unsigned int on)
{
	if ( on ) {
		return asic_clear_bits(cci, RC632_REG_CONTROL,
						RC632_CONTROL_POWERDOWN);
	}else{
		return asic_set_bits(cci, RC632_REG_CONTROL,
						RC632_CONTROL_POWERDOWN);
	}
}

int _clrc632_rf_power(struct _cci *cci, unsigned int on)
{
	if ( on ) {
		return asic_clear_bits(cci, RC632_REG_TX_CONTROL,
				RC632_TXCTRL_TX1_RF_EN|RC632_TXCTRL_TX2_RF_EN);
	}else{
		return asic_set_bits(cci, RC632_REG_TX_CONTROL,
				RC632_TXCTRL_TX1_RF_EN|RC632_TXCTRL_TX2_RF_EN);
	}
}

static int flush_fifo(struct _cci *cci)
{
	return reg_write(cci, RC632_REG_CONTROL, RC632_CONTROL_FIFO_FLUSH);
}

static const struct reg_file rf_14443a_init[] = {
	{ .reg =	RC632_REG_TX_CONTROL,
	  .val =	RC632_TXCTRL_MOD_SRC_INT |
			RC632_TXCTRL_TX2_INV |
			RC632_TXCTRL_FORCE_100_ASK |
			RC632_TXCTRL_TX2_RF_EN |
			RC632_TXCTRL_TX1_RF_EN },
	{ .reg = 	RC632_REG_CW_CONDUCTANCE,
	  .val = 	0x3f },
	{ .reg = 	RC632_REG_MOD_CONDUCTANCE,
	  .val = 	0x3f },
	{ .reg = 	RC632_REG_CODER_CONTROL,
	  .val = 	RC632_CDRCTRL_TXCD_14443A |
	  		RC632_CDRCTRL_RATE_106K },
	{ .reg = 	RC632_REG_MOD_WIDTH,
	  .val = 	0x13 },
	{ .reg = 	RC632_REG_MOD_WIDTH_SOF,
	  .val = 	0x3f },
	{ .reg = 	RC632_REG_TYPE_B_FRAMING,
	  .val = 	0 },
	{ .reg = 	RC632_REG_RX_CONTROL1,
	  .val = 	RC632_RXCTRL1_GAIN_35DB |
	  		RC632_RXCTRL1_ISO14443 |
			RC632_RXCTRL1_SUBCP_8},
	{ .reg = 	RC632_REG_DECODER_CONTROL,
	  .val = 	RC632_DECCTRL_MANCHESTER |
	  		RC632_DECCTRL_RXFR_14443A },
	{ .reg = 	RC632_REG_BIT_PHASE,
	  .val = 	0xa9 },
	{ .reg = 	RC632_REG_RX_THRESHOLD,
	  .val = 	0xff },
	{ .reg = 	RC632_REG_BPSK_DEM_CONTROL,
	  .val = 	0 },
	{ .reg = 	RC632_REG_RX_CONTROL2,
	  .val = 	RC632_RXCTRL2_DECSRC_INT |
	  		RC632_RXCTRL2_CLK_Q },
	{ .reg = 	RC632_REG_RX_WAIT,
	  .val = 	6 },
	{ .reg = 	RC632_REG_CHANNEL_REDUNDANCY,
	  .val = 	RC632_CR_PARITY_ENABLE |
	  		RC632_CR_PARITY_ODD },
	{ .reg =	RC632_REG_CRC_PRESET_LSB,
	  .val = 	0x63 },
	{ .reg = 	RC632_REG_CRC_PRESET_MSB,
	  .val =	0x63 },
};

int _clrc632_14443a_init(struct _cci *cci)
{
	if ( !flush_fifo(cci) )
		return 0;
	return reg_write_batch(cci, rf_14443a_init,
				ARRAY_SIZE(rf_14443a_init));
}

int _clrc632_select(struct _cci *cci)
{
	return _iso14443a_select(cci, 0);
}

int _clrc632_init(struct _cci *cci)
{
	if ( !asic_power(cci, 0) )
		return 0;

	usleep(1000);

	if ( !asic_power(cci, 1) )
		return 0;

	if ( !asic_set_bits(cci, RC632_REG_PAGE0, 0) )
		return 0;
	if ( !asic_set_bits(cci, RC632_REG_TX_CONTROL, 0x5b) )
		return 0;

	if ( !_clrc632_rf_power(cci, 0) )
		return 0;
	usleep(1000);
	if ( !_clrc632_rf_power(cci, 1) )
		return 0;

	return 1;
}
