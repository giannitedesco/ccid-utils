/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/
#ifndef _CCID_H
#define _CCID_H

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "compiler.h"
#include "bytesex.h"

/** \mainpage Chip Card Interface Device (CCID) Utilities.
  * \section intro Introduction.
  * blah blah...
  * \section code Code Overview
  *  -# First find a device.
  *  -# Then open it.
  *  -# Activate a chip card.
  *  -# Create a transaction buffer.
  *  -# Submit transactions to the chip card.
  *  -# Close connection to chip card device.
  */

/**
 * \defgroup g_ccid CCID Library
 * Generic top level functions of libccid.
 *
 * \defgroup g_cci Chip Card Interface Device (CCID).
 * Represents a connection to a chip card device.
 *
 * \defgroup g_xfr CCID Transaction Buffer
 * Provides:
 *
 *  -# An appropriately sized send/receive buffer for a CCID.
 *  -# An interface for constructing requests.
 *  -# An interface for retreiving status words and retrieved data.
 *
 * \defgroup g_chipcard Chip Card
 * Represents a slot in a chip card device and chip card (if one is inserted).
 */

/** \ingroup g_cci
 * Chip Card Interface Device
*/
typedef struct _cci *cci_t;

/** \ingroup g_chipcard
 * Chip Card
*/
typedef struct _chipcard *chipcard_t;

/** \ingroup g_xfr
 * Transaction Buffer
*/
typedef struct _xfr *xfr_t;

/** \ingroup g_ccid
 * Physical Chip Card Interface Device.
 *
 * A name referencing an unopened hardware device
*/
typedef struct usb_device *ccidev_t;

_public ccidev_t ccid_find_first_device(void);
//ccidev_t ccid_find_next_device(void);

_public cci_t cci_probe(ccidev_t dev, const char *tracefile);
_public unsigned int cci_slots(cci_t cci);
_public chipcard_t cci_get_slot(cci_t cci, unsigned int i);
_public void cci_close(cci_t cci);
_public void cci_log(cci_t cci, const char *fmt, ...) _printf(2, 3);

_public xfr_t  xfr_alloc(size_t txbuf, size_t rxbuf);
_public void xfr_reset(xfr_t xfr);
_public int xfr_tx_byte(xfr_t xfr, uint8_t byte);
_public int xfr_tx_buf(xfr_t xfr, const uint8_t *ptr, size_t len);

_public uint8_t xfr_rx_sw1(xfr_t xfr);
_public uint8_t xfr_rx_sw2(xfr_t xfr);
_public const uint8_t *xfr_rx_data(xfr_t xfr, size_t *len);

_public void xfr_free(xfr_t xfr);

_public cci_t chipcard_cci(chipcard_t cc);
_public int chipcard_wait_for_card(chipcard_t cc);

/** \ingroup g_chipcard
 * Chip card is present in the slot, powered and clocked.
*/
#define CHIPCARD_ACTIVE		0x0
/** \ingroup g_chipcard
 * Chip card is present in the slot but not powered or clocked.
*/
#define CHIPCARD_PRESENT	0x1
/** \ingroup g_chipcard
 * Chip card is not present in the slot.
*/
#define CHIPCARD_NOT_PRESENT	0x2
_public unsigned int chipcard_status(chipcard_t cc);

/** \ingroup g_chipcard
 * There was an error while retrieving clock status.
*/
#define CHIPCARD_CLOCK_ERR	0x0
/** \ingroup g_chipcard
 * Clock is started.
*/
#define CHIPCARD_CLOCK_START	0x1
/** \ingroup g_chipcard
 * Clock signal is stopped low.
*/
#define CHIPCARD_CLOCK_STOP_L	0x2
/** \ingroup g_chipcard
 * Clock is stopped high.
*/
#define CHIPCARD_CLOCK_STOP_H	0x3
/** \ingroup g_chipcard
 * Clock is stopped in unknown state.
*/
#define CHIPCARD_CLOCK_STOP	0x4
_public unsigned int chipcard_slot_status(chipcard_t cc);

/** \ingroup g_chipcard
 * Automatically select chip card voltage.
*/
#define CHIPCARD_AUTO_VOLTAGE	0x0
/** \ingroup g_chipcard
 * 5 Volts.
*/
#define CHIPCARD_5V		0x1
/** \ingroup g_chipcard
 * 3 Volts.
*/
#define CHIPCARD_3V		0x2
/** \ingroup g_chipcard
 * 1.8 Volts.
*/
#define CHIPCARD_1_8V		0x3
_public const uint8_t *chipcard_slot_on(chipcard_t cc, unsigned int voltage,
				size_t *atr_len);
_public int chipcard_slot_off(chipcard_t cc);

_public int chipcard_transact(chipcard_t cc, xfr_t xfr);

/* -- Utility functions */
_public void hex_dump(const uint8_t *ptr, size_t len, size_t llen);
_public void hex_dumpf(FILE *f, const uint8_t *ptr, size_t len, size_t llen);
_public void ber_dump(const uint8_t *ptr, size_t len, unsigned int depth);

#endif /* _CCID_H */
