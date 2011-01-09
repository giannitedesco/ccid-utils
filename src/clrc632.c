/*
 * This file is part of cci-utils
 * Copyright (c) 2010 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
 *
 * Chip card transaction buffer management.
*/

#include <ccid.h>
#include <unistd.h>

#include "ccid-internal.h"
#include "clrc632.h"

static int reg_read(struct _chipcard *cc, unsigned int reg, uint8_t *val)
{
	return (*cc->cc_rc632->reg_read)(cc->cc_parent, cc->cc_idx, reg, val);
}

static int reg_write(struct _chipcard *cc, unsigned int reg, uint8_t val)
{
	return (*cc->cc_rc632->reg_write)(cc->cc_parent, cc->cc_idx, reg, val);
}

static int asic_clear_bits(struct _chipcard *cc, unsigned int reg, uint8_t bits)
{
	uint8_t val;

	if ( !reg_read(cc, reg, &val) )
		return 0;
	
	if ( (val & bits) == 0 )
		return 1;
	
	return reg_write(cc, reg, val & ~bits);
}

static int asic_set_bits(struct _chipcard *cc, unsigned int reg, uint8_t bits)
{
	uint8_t val;

	if ( !reg_read(cc, reg, &val) )
		return 0;
	
	if ( (val & bits) == bits )
		return 1;
	
	return reg_write(cc, reg, val | bits);
}

static int asic_power(struct _chipcard *cc, unsigned int on)
{
	if ( on ) {
		return asic_clear_bits(cc, RC632_REG_CONTROL,
						RC632_CONTROL_POWERDOWN);
	}else{
		return asic_set_bits(cc, RC632_REG_CONTROL,
						RC632_CONTROL_POWERDOWN);
	}
}

static int asic_rf_power(struct _chipcard *cc, unsigned int on)
{
	if ( on ) {
		return asic_clear_bits(cc, RC632_REG_TX_CONTROL,
				RC632_TXCTRL_TX1_RF_EN|RC632_TXCTRL_TX2_RF_EN);
	}else{
		return asic_set_bits(cc, RC632_REG_TX_CONTROL,
				RC632_TXCTRL_TX1_RF_EN|RC632_TXCTRL_TX2_RF_EN);
	}
}

int _clrc632_init(struct _chipcard *cc)
{
	if ( !asic_power(cc, 0) )
		return 0;
	usleep(1000);
	if ( !asic_power(cc, 1) )
		return 0;
	
	if ( !asic_set_bits(cc, RC632_REG_PAGE, 0) )
		return 0;
	if ( !asic_set_bits(cc, RC632_REG_TX_CONTROL, 0x5b) )
		return 0;

	if ( !asic_rf_power(cc, 0) )
		return 0;
	usleep(1000);
	if ( !asic_rf_power(cc, 1) )
		return 0;

	return 1;
}
