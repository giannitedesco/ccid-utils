/*
 * This file is part of ccid-utils
 * Copyright (c) 2010 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
 *
 * CLRC632 RFID ASIC driver
 *
 * Much logic liberally copied from librfid
 * (C) 2005-2008 Harald Welte <laforge@gnumonks.org>
*/

#include <ccid.h>
#include <unistd.h>
#include <inttypes.h>

#include "ccid-internal.h"
#include "clrc632-regs.h"
#include "rfid.h"
#include "clrc632.h"
#include "rfid_layer1.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x)/sizeof(*x))
#endif

struct reg_file {
	uint8_t reg;
	uint8_t val;
};

static int reg_read(struct _ccid *ccid, const struct _clrc632_ops *ops,
			uint8_t reg, uint8_t *val)
{
	return (*ops->reg_read)(ccid, reg, val);
}

static int reg_write(struct _ccid *ccid, const struct _clrc632_ops *ops,
			uint8_t reg, uint8_t val)
{
	return (*ops->reg_write)(ccid, reg, val);
}

static int fifo_read(struct _ccid *ccid, const struct _clrc632_ops *ops,
			uint8_t *buf, size_t len)
{
	return (*ops->fifo_read)(ccid, buf, len);
}

static int fifo_write(struct _ccid *ccid, const struct _clrc632_ops *ops,
			const uint8_t *buf, size_t len)
{
	return (*ops->fifo_write)(ccid, buf, len);
}

static int asic_clear_bits(struct _ccid *ccid, void *priv,
				uint8_t reg, uint8_t bits)
{
	uint8_t val;

	if ( !reg_read(ccid, priv, reg, &val) )
		return 0;

	if ( (val & bits) == 0 )
		return 1;

	return reg_write(ccid, priv, reg, val & ~bits);
}

static int asic_set_bits(struct _ccid *ccid, void *priv,
			 uint8_t reg, uint8_t bits)
{
	uint8_t val;

	if ( !reg_read(ccid, priv, reg, &val) )
		return 0;

	if ( (val & bits) == bits )
		return 1;

	return reg_write(ccid, priv, reg, val | bits);
}

static int asic_set_mask(struct _ccid *ccid, void *priv, uint8_t reg,
			 uint8_t mask, uint8_t bits)
{
	uint8_t val;

	if ( !reg_read(ccid, priv, reg, &val) )
		return 0;

	if ( (val & mask) == bits )
		return 1;

	return reg_write(ccid, priv, reg, (val & ~mask) | (bits & mask));
}

static int reg_write_batch(struct _ccid *ccid, void *priv,
				const struct reg_file *r,
				unsigned int num)
{
	unsigned int i;
	for(i = 0; i < num; i++) {
		if ( !reg_write(ccid, priv, r[i].reg, r[i].val) )
			return 0;
	}
	return 1;
}

static int asic_power(struct _ccid *ccid, void *priv, unsigned int on)
{
	if ( on ) {
		return asic_clear_bits(ccid, priv, RC632_REG_CONTROL,
						RC632_CONTROL_POWERDOWN);
	}else{
		return asic_set_bits(ccid, priv, RC632_REG_CONTROL,
						RC632_CONTROL_POWERDOWN);
	}
}

static int rf_power(struct _ccid *ccid, void *priv, unsigned int on)
{
	int ret;

	if ( on ) {
		ret = asic_clear_bits(ccid, priv, RC632_REG_TX_CONTROL,
				RC632_TXCTRL_TX1_RF_EN|RC632_TXCTRL_TX2_RF_EN);
	}else{
		ret = asic_set_bits(ccid, priv, RC632_REG_TX_CONTROL,
				RC632_TXCTRL_TX1_RF_EN|RC632_TXCTRL_TX2_RF_EN);
	}

	/* let it settle */
	if ( ret )
		usleep(10000);

	return ret;
}

static int flush_fifo(struct _ccid *ccid, void *priv)
{
	return reg_write(ccid, priv, RC632_REG_CONTROL,
				RC632_CONTROL_FIFO_FLUSH);
}

static int clear_irqs(struct _ccid *ccid, void *priv, uint8_t bits)
{
	return reg_write(ccid, priv, RC632_REG_INTERRUPT_RQ,
			(~RC632_INT_SET) & bits);
}

/* Wait until RC632 is idle or TIMER IRQ has happened */
static int wait_idle_timer(struct _ccid *ccid, void *priv)
{
	uint8_t stat, irq, cmd;

	if ( !reg_read(ccid, priv, RC632_REG_INTERRUPT_EN, &irq) )
		return 0;

	if ( !reg_write(ccid, priv, RC632_REG_INTERRUPT_EN, RC632_IRQ_SET
				| RC632_IRQ_TIMER
				| RC632_IRQ_IDLE
				| RC632_IRQ_RX ) )
		return 0;

	while (1) {
		if ( !reg_read(ccid, priv, RC632_REG_PRIMARY_STATUS, &stat) )
			return 0;
		if (stat & RC632_STAT_ERR) {
			uint8_t err;
			if ( !reg_read(ccid, priv, RC632_REG_ERROR_FLAG, &err) )
				return 0;
			if (err & (RC632_ERR_FLAG_COL_ERR |
				   RC632_ERR_FLAG_PARITY_ERR |
				   RC632_ERR_FLAG_FRAMING_ERR |
				/* FIXME: why get we CRC errors in CL2 anticol
				 * at iso14443a operation with mifare UL? */
				/*   RC632_ERR_FLAG_CRC_ERR | */
				   0)) {
				printf("error during wait\n");
				return 0;
			}
		}
		if (stat & RC632_STAT_IRQ) {
			if ( !reg_read(ccid, priv,
				RC632_REG_INTERRUPT_RQ, &irq) )
				return 0;

			if (irq & RC632_IRQ_TIMER && !(irq & RC632_IRQ_RX)) {
				/* timed out */
				printf("..timed out\n");
				clear_irqs(ccid, priv, RC632_IRQ_TIMER);
				return 0;
			}
		}

		if ( !reg_read(ccid, priv, RC632_REG_COMMAND, &cmd) )
			return 0;

		if (cmd == 0) {
			clear_irqs(ccid, priv, RC632_IRQ_RX);
			return 1;
		}

		/* poll every millisecond */
		usleep(1000);
	}
}

/* calculate best 8bit prescaler and divisor for given usec timeout */
static void best_prescaler(uint64_t timeout, uint8_t *prescaler,
			  uint8_t *divisor)
{
	uint8_t best_prescaler, best_divisor, i;
	int64_t smallest_diff;

	smallest_diff = LLONG_MAX;
	best_prescaler = 0;

	for (i = 0; i < 21; i++) {
		uint64_t clk, tmp_div, res;
		int64_t diff;
		clk = 13560000 / (1 << i);
		tmp_div = (clk * timeout) / 1000000;
		tmp_div++;

		if ((tmp_div > 0xff) || (tmp_div > clk))
			continue;

		res = 1000000 / (clk / tmp_div);
		diff = res - timeout;

		if (diff < 0)
			continue;

		if (diff < smallest_diff) {
			best_prescaler = i;
			best_divisor = tmp_div;
			smallest_diff = diff;
		}
	}

	*prescaler = best_prescaler;
	*divisor = best_divisor;

//	printf("%s: timeout %"PRIu64" usec, prescaler = %u, divisor = %u\n",
//		__func__, timeout, best_prescaler, best_divisor);
}

#define TIMER_RELAX_FACTOR 10
static int timer_set(struct _ccid *ccid, void *priv, uint64_t timeout)
{
	uint8_t prescaler, divisor;

	timeout *= TIMER_RELAX_FACTOR;

	best_prescaler(timeout, &prescaler, &divisor);

	if ( !reg_write(ccid, priv, RC632_REG_TIMER_CLOCK,
			      prescaler & 0x1f) )
		return 0;

	if ( !reg_write(ccid, priv, RC632_REG_TIMER_CONTROL,
			      RC632_TMR_START_TX_END|RC632_TMR_STOP_RX_BEGIN) )
		return 0;

	/* clear timer irq bit */
	if ( !clear_irqs(ccid, priv, RC632_IRQ_TIMER) )
		return 0;

	/* enable timer IRQ */
	if ( !reg_write(ccid, priv, RC632_REG_INTERRUPT_EN,
			RC632_IRQ_SET | RC632_IRQ_TIMER) )
		return 0;

	if ( !reg_write(ccid, priv, RC632_REG_TIMER_RELOAD, divisor) )
		return 0;

	return 1;
}

static int set_rf_mode(struct _ccid *ccid, void *priv, const struct rf_mode *rf)
{
	uint8_t red;

	if ( !reg_write(ccid, priv, RC632_REG_BIT_FRAMING,
				(rf->rx_align << 4) | (rf->tx_last_bits)) )
		return 0;

	if ( rf->flags & RF_CRYPTO1 ) {
		if ( !asic_clear_bits(ccid, priv, RC632_REG_CONTROL,
					RC632_CONTROL_CRYPTO1_ON) )
			return 0;
	}else{
		if ( !asic_set_bits(ccid, priv, RC632_REG_CONTROL,
					RC632_CONTROL_CRYPTO1_ON) )
			return 0;
	}

	red = 0;
	if ( rf->flags & RF_TX_CRC )
		red |= RC632_CR_TX_CRC_ENABLE;
	if ( rf->flags & RF_RX_CRC )
		red |= RC632_CR_RX_CRC_ENABLE;
	if ( rf->flags & RF_PARITY_ENABLE )
		red |= RC632_CR_PARITY_ENABLE;
	if ( !(rf->flags & RF_PARITY_EVEN) )
		red |= RC632_CR_PARITY_ODD;
	
	if ( !reg_write(ccid, priv, RC632_REG_CHANNEL_REDUNDANCY, red) )
		return 0;

	return 1;
}

static int get_rf_mode(struct _ccid *ccid, void *priv, const struct rf_mode *rf)
{
	return 0;
}

static int get_error(struct _ccid *ccid, void *priv, uint8_t *err)
{
	return reg_read(ccid, priv, RC632_REG_ERROR_FLAG, err);
}

static int get_coll_pos(struct _ccid *ccid, void *priv, uint8_t *pos)
{
	return reg_read(ccid, priv, RC632_REG_COLL_POS, pos);
}

static int transact(struct _ccid *ccid, void *priv,
			 const uint8_t *tx_buf,
			 uint8_t tx_len,
			 uint8_t *rx_buf,
			 uint8_t *rx_len,
			 uint64_t timer,
			 unsigned int toggle)
{
	int cur_tx_len;
	uint8_t rx_avail;
	const uint8_t *cur_tx_buf = tx_buf;

//	printf("%s: timeout=%"PRIu64", rx_len=%u, tx_len=%u\n",
//		__func__, timer, *rx_len, tx_len);

	if (tx_len > 64)
		cur_tx_len = 64;
	else
		cur_tx_len = tx_len;

	if ( !reg_write(ccid, priv, RC632_REG_COMMAND, RC632_CMD_IDLE) )
		return 0;
	/* clear all interrupts */
	if ( !reg_write(ccid, priv, RC632_REG_INTERRUPT_RQ, 0x7f) )
		return 0;

	if ( !timer_set(ccid, priv, timer) )
		return 0;

	do {
		if ( !fifo_write(ccid, priv, cur_tx_buf, cur_tx_len) )
			return 0;

		if (cur_tx_buf == tx_buf) {
			if ( !reg_write(ccid, priv, RC632_REG_COMMAND,
					RC632_CMD_TRANSCEIVE) )
				return 0;
		}

		cur_tx_buf += cur_tx_len;
		if (cur_tx_buf < tx_buf + tx_len) {
			uint8_t fifo_fill;
			if ( !reg_read(ccid, priv, RC632_REG_FIFO_LENGTH,
					&fifo_fill) )
				return 0;

			cur_tx_len = 64 - fifo_fill;
		} else
			cur_tx_len = 0;
	} while (cur_tx_len);

	//if (toggle == 1)
	//	tcl_toggle_pcb(ccid, priv);

	if ( !wait_idle_timer(ccid, priv) ) {
		return 0;
	}

	if ( !reg_read(ccid, priv, RC632_REG_FIFO_LENGTH, &rx_avail) )
		return 0;

	if (rx_avail > *rx_len)
		printf("rx_avail(%d) > rx_len(%d), JFYI\n", rx_avail, *rx_len);
	else if (*rx_len > rx_avail)
		*rx_len = rx_avail;

	if (rx_avail == 0) {
		printf("RX: FIFO empty\n");
		return 0;
	}

	/* FIXME: discard addidional bytes in FIFO */
	return fifo_read(ccid, priv, rx_buf, *rx_len);
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

static int iso14443a_init(struct _ccid *ccid, void *priv)
{
	if ( !flush_fifo(ccid, priv) )
		return 0;
	return reg_write_batch(ccid, priv, rf_14443a_init,
				ARRAY_SIZE(rf_14443a_init));
}

static struct {
	uint8_t subc_pulses;
	uint8_t rx_coding;
	uint8_t rx_threshold;
	uint8_t bpsk_dem_ctrl;
	uint8_t rate;
	uint8_t mod_width;
}rate[] = {
	[RFID_14443A_SPEED_106K] = {
	  .subc_pulses 		= RC632_RXCTRL1_SUBCP_8,
	  .rx_coding 		= RC632_DECCTRL_MANCHESTER,
	  .rx_threshold		= 0x88,
	  .bpsk_dem_ctrl	= 0x00,
	  .rate 		= RC632_CDRCTRL_RATE_106K,
	  .mod_width		= 0x13,
	},
	[RFID_14443A_SPEED_212K] = {
	  .subc_pulses		= RC632_RXCTRL1_SUBCP_4,
	  .rx_coding		= RC632_DECCTRL_BPSK,
	  .rx_threshold		= 0x50,
	  .bpsk_dem_ctrl	= 0x0c,
	  .rate			= RC632_CDRCTRL_RATE_212K,
	  .mod_width		= 0x07,
	},
	[RFID_14443A_SPEED_424K] = { 
	  .subc_pulses		= RC632_RXCTRL1_SUBCP_2,
	  .rx_coding		= RC632_DECCTRL_BPSK,
	  .rx_threshold		= 0x50,
	  .bpsk_dem_ctrl	= 0x0c,
	  .rate			= RC632_CDRCTRL_RATE_424K,
	  .mod_width		= 0x03,
	},
	[RFID_14443A_SPEED_848K] = { 
	  .subc_pulses		= RC632_RXCTRL1_SUBCP_1,
	  .rx_coding		= RC632_DECCTRL_BPSK,
	  .rx_threshold		= 0x50,
	  .bpsk_dem_ctrl	= 0x0c,
	  .rate			= RC632_CDRCTRL_RATE_848K,
	  .mod_width		= 0x01,
	},
};

static int set_speed(struct _ccid *ccid, void *priv, unsigned int i)
{
	if ( i >= ARRAY_SIZE(rate) )
		return 0;
	
	if ( !asic_set_mask(ccid, priv, RC632_REG_RX_CONTROL1,
			   RC632_RXCTRL1_SUBCP_MASK,
			   rate[i].subc_pulses) )
		return 0;

	if ( !asic_set_mask(ccid, priv, RC632_REG_DECODER_CONTROL,
			   RC632_DECCTRL_BPSK,
			   rate[i].rx_coding) )
		return 0;

	if ( !reg_write(ccid, priv, RC632_REG_RX_THRESHOLD, rate[i].rx_threshold) )
		return 0;

	if ( rate[i].rx_coding == RC632_DECCTRL_BPSK &&
		!reg_write(ccid, priv, RC632_REG_BPSK_DEM_CONTROL,
				rate[i].bpsk_dem_ctrl) )
		return 0;

	if ( !asic_set_mask(ccid, priv, RC632_REG_CODER_CONTROL,
			RC632_CDRCTRL_RATE_MASK,
			rate[i].rate) )
		return 0;

	if ( !reg_write(ccid, priv, RC632_REG_MOD_WIDTH, rate[i].mod_width) )
		return 0;

	return 1;
}

static unsigned int get_speeds(struct _ccid *ccid, void *priv)
{
	return (1 << RFID_14443A_SPEED_106K) |
		(1 << RFID_14443A_SPEED_212K) |
		(1 << RFID_14443A_SPEED_424K) |
		(1 << RFID_14443A_SPEED_848K);
}

static unsigned int carrier_freq(struct _ccid *ccid, void *priv)
{
	return ISO14443_FREQ_CARRIER;
}

static unsigned int get_mtu(struct _ccid *ccid, void *priv)
{
	return 64;
}

static unsigned int get_mru(struct _ccid *ccid, void *priv)
{
	return 64;
}

static const struct rfid_layer1_ops l1_ops = {
	.rf_power = rf_power,
	.iso14443a_init = iso14443a_init,
	.set_rf_mode = set_rf_mode, 
	.get_rf_mode = get_rf_mode,
	.get_error = get_error,
	.get_coll_pos = get_coll_pos,
	.set_speed = set_speed,
	.transact = transact,
	.carrier_freq = carrier_freq,
	.get_speeds = get_speeds,
	.mtu = get_mtu,
	.mru = get_mru,
};

int _clrc632_init(struct _cci *cci, const struct _clrc632_ops *asic_ops)
{
	struct _ccid *ccid = cci->cc_parent;
	void *priv = (void *)asic_ops;

	if ( !asic_power(ccid, priv, 0) )
		return 0;

	usleep(10000);

	if ( !asic_power(ccid, priv, 1) )
		return 0;

	if ( !asic_set_bits(ccid, priv, RC632_REG_PAGE0, 0) )
		return 0;
	if ( !asic_set_bits(ccid, priv, RC632_REG_TX_CONTROL, 0x5b) )
		return 0;

	if ( !_rfid_init(cci, &l1_ops, priv) )
		return 0;

	return 1;
}
