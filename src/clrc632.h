/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/
#ifndef _CLRC632_H
#define _CLRC632_H

/* CL RC632 register set */

#define RC632_REG_PAGE		0x00
#define RC632_REG_CONTROL	0x09
#define  RC632_CONTROL_POWERDOWN	(1<<4)
#define RC632_REG_TX_CONTROL	0x11
#define  RC632_TXCTRL_TX1_RF_EN		(1<<0)
#define  RC632_TXCTRL_TX2_RF_EN		(1<<1)

#endif /* _CLRC632_H */
