/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/
#ifndef _CLRC632_H
#define _CLRC632_H

_private int _clrc632_init(struct _cci *cc);
_private int _clrc632_rf_power(struct _cci *cci, unsigned int on);
_private int _clrc632_14443a_init(struct _cci *cci);
_private int _clrc632_select(struct _cci *cci);

/* CL RC632 register set */
/* PAGE 0 */
#define RC632_REG_PAGE0			0x00

#define RC632_REG_COMMAND		0x01
#define  RC632_CMD_IDLE			0
#define  RC632_CMD_TRANSCEIVE		0x1e

#define RC632_REG_FIFO_DATA		0x02

#define RC632_REG_PRIMARY_STATUS	0x03
#define  RC632_STAT_LOALERT		(1<<0)
#define  RC632_STAT_HIALERT		(1<<1)
#define  RC632_STAT_ERR			(1<<2)
#define  RC632_STAT_IRQ			(1<<3)

#define RC632_REG_FIFO_LENGTH		0x04
#define RC632_REG_SECONDARY_STATUS	0x05

#define RC632_REG_INTERRUPT_EN		0x06
#define  RC632_IRQ_LO_ALERT		(1<<0)
#define  RC632_IRQ_HI_ALERT		(1<<1)
#define  RC632_IRQ_IDLE			(1<<2)
#define  RC632_IRQ_RX			(1<<3)
#define  RC632_IRQ_TX			(1<<4)
#define  RC632_IRQ_TIMER		(1<<5)
#define  RC632_IRQ_SET			(1<<7)

#define RC632_REG_INTERRUPT_RQ		0x07
#define  RC632_INT_LO_ALERT		(1<<0)
#define  RC632_INT_HI_ALERT		(1<<1)
#define  RC632_INT_IDLE			(1<<2)
#define  RC632_INT_RX			(1<<3)
#define  RC632_INT_TX			(1<<4)
#define  RC632_INT_TIMER		(1<<5)
#define  RC632_INT_SET			(1<<7)

/* PAGE 1 */
#define RC632_REG_PAGE1			0x08

#define RC632_REG_CONTROL		0x09
#define  RC632_CONTROL_FIFO_FLUSH	(1<<0)
#define  RC632_CONTROL_TIMER_START	(1<<1)
#define  RC632_CONTROL_TIMER_STOP	(1<<2)
#define  RC632_CONTROL_CRYPTO1_ON	(1<<3)
#define  RC632_CONTROL_POWERDOWN	(1<<4)
#define  RC632_CONTROL_STANDBY		(1<<5)

#define RC632_REG_ERROR_FLAG		0x0a
#define  RC632_ERR_FLAG_COL_ERR		(1<<0)
#define  RC632_ERR_FLAG_PARITY_ERR	(1<<1)
#define  RC632_ERR_FLAG_FRAMING_ERR	(1<<2)
#define  RC632_ERR_FLAG_CRC_ERR		(1<<3)
#define  RC632_ERR_FLAG_FIFO_OVERFLOW	(1<<4)
#define  RC632_ERR_FLAG_ACCESS_ERR	(1<<5)
#define  RC632_ERR_FLAG_KEY_ERR		(1<<6)

#define RC632_REG_COLL_POS		0x0b
#define RC632_REG_TIMER_VALUE		0x0c
#define RC632_REG_CRC_RESULT_LSB	0x0d
#define RC632_REG_CRC_RESULT_MSB	0x0e
#define RC632_REG_BIT_FRAMING		0x0f

/* PAGE 2 */
#define RC632_REG_PAGE2			0x10

#define RC632_REG_TX_CONTROL		0x11
#define  RC632_TXCTRL_TX1_RF_EN		(1<<0)
#define  RC632_TXCTRL_TX2_RF_EN		(1<<1)
#define  RC632_TXCTRL_TX2_CW		(1<<2)
#define  RC632_TXCTRL_TX2_INV		(1<<3)
#define  RC632_TXCTRL_FORCE_100_ASK	(1<<4)
#define  RC632_TXCTRL_MOD_SRC_LO	(0<<5)
#define  RC632_TXCTRL_MOD_SRC_HI	(1<<5)
#define  RC632_TXCTRL_MOD_SRC_INT	(2<<5)
#define  RC632_TXCTRL_MOD_SRC_MFIN	(3<<5)

#define RC632_REG_CW_CONDUCTANCE	0x12
#define RC632_REG_MOD_CONDUCTANCE	0x13

#define RC632_REG_CODER_CONTROL		0x14
#define  RC632_CDRCTRL_TXCD_14443A	(1)
#define  RC632_CDRCTRL_RATE_106K	(3 << 3)

#define RC632_REG_MOD_WIDTH		0x15
#define RC632_REG_MOD_WIDTH_SOF		0x16
#define RC632_REG_TYPE_B_FRAMING	0x17

/* PAGE 3 */
#define RC632_REG_PAGE3			0x18

#define RC632_REG_RX_CONTROL1		0x19
#define  RC632_RXCTRL1_GAIN_20DB	(0)
#define  RC632_RXCTRL1_GAIN_24DB	(1)
#define  RC632_RXCTRL1_GAIN_31DB	(2)
#define  RC632_RXCTRL1_GAIN_35DB	(3)
#define  RC632_RXCTRL1_ISO14443		(2 << 3)
#define  RC632_RXCTRL1_SUBCP_1		(0 << 5)
#define  RC632_RXCTRL1_SUBCP_2		(1 << 5)
#define  RC632_RXCTRL1_SUBCP_4		(2 << 5)
#define  RC632_RXCTRL1_SUBCP_8		(3 << 5)
#define  RC632_RXCTRL1_SUBCP_16		(4 << 5)

#define RC632_REG_DECODER_CONTROL	0x1a
#define  RC632_DECCTRL_MANCHESTER	(0)
#define  RC632_DECCTRL_RXFR_14443A	(1 << 3)

#define RC632_REG_BIT_PHASE		0x1b
#define RC632_REG_RX_THRESHOLD		0x1c
#define RC632_REG_BPSK_DEM_CONTROL	0x1d

#define RC632_REG_RX_CONTROL2		0x1e
#define  RC632_RXCTRL2_DECSRC_LOW	(0)
#define  RC632_RXCTRL2_DECSRC_INT	(1)
#define  RC632_RXCTRL2_DECSRC_SC	(2)
#define  RC632_RXCTRL2_DECSRC_BB	(3)
#define  RC632_RXCTRL2_CLK_Q		(0<<7)
#define  RC632_RXCTRL2_CLK_I		(1<<7)

#define RC632_REG_CLOCK_Q_CONTROL	0x1f

/* PAGE 4 */
#define RC632_REG_PAGE4			0x20
#define RC632_REG_RX_WAIT		0x21

#define RC632_REG_CHANNEL_REDUNDANCY	0x22
#define  RC632_CR_PARITY_EVEN		(0<<1)
#define  RC632_CR_PARITY_ENABLE		(1<<0)
#define  RC632_CR_PARITY_ODD		(1<<1)
#define  RC632_CR_TX_CRC_ENABLE		(1<<2)
#define  RC632_CR_RX_CRC_ENABLE		(1<<3)
#define  RC632_CR_CRC8			(1<<4)
#define  RC632_CR_CRC3309		(1<<5)

#define RC632_REG_CRC_PRESET_LSB	0x23
#define RC632_REG_CRC_PRESET_MSB	0x24
#define RC632_REG_TIME_SLOT_PERIOD	0x25
#define RC632_REG_MFOUT_SELECT		0x26
#define RC632_REG_PRESET_27		0x27

#define RC632_REG_PAGE5			0x28
#define RC632_REG_FIFO_LEVEL		0x29
#define RC632_REG_TIMER_CLOCK		0x2a

#define RC632_REG_TIMER_CONTROL		0x2b
#define  RC632_TMR_START_TX_BEGIN	(1<<0)
#define  RC632_TMR_START_TX_END		(1<<1)
#define  RC632_TMR_STOP_RX_BEGIN	(1<<2)
#define  RC632_TMR_STOP_RX_END		(1<<3)

#define RC632_REG_TIMER_RELOAD		0x2c
#define RC632_REG_IRQ_PIN_CONFIG	0x2d
#define RC632_REG_PRESET_2E		0x2e
#define RC632_REG_PRESET_2F		0x2f

#define RC632_REG_PAGE6			0x30

#define RC632_REG_PAGE7			0x38
#define RC632_REG_TEST_ANA_SELECT	0x3a
#define RC632_REG_TEST_DIGI_SELECT	0x3d

#endif /* _CLRC632_H */
