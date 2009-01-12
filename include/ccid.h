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

typedef struct _cci *cci_t;
typedef struct _chipcard *chipcard_t;
typedef struct _xfr *xfr_t;
typedef struct usb_device *ccidev_t;

/* Chipcard interface devices */
ccidev_t ccid_find_first_device(void);
//ccidev_t ccid_find_next_device(void);

/* -- Chipcard interface */
cci_t cci_probe(ccidev_t dev, const char *tracefile);
unsigned int cci_slots(cci_t cci);
chipcard_t cci_get_slot(cci_t cci, unsigned int i);
void cci_close(cci_t cci);

/* -- Xfr */
xfr_t  xfr_alloc(size_t txbuf, size_t rxbuf);
void xfr_reset(xfr_t xfr);
int xfr_tx_byte(xfr_t xfr, uint8_t byte);
int xfr_tx_buf(xfr_t xfr, const uint8_t *ptr, size_t len);

/* Only valid if chipcard_transact() returns 1 */
uint8_t xfr_rx_sw1(xfr_t xfr);
uint8_t xfr_rx_sw2(xfr_t xfr);
const uint8_t *xfr_rx_data(xfr_t xfr, size_t *len);

void xfr_free(xfr_t xfr);

/* -- Chipcard */
cci_t chipcard_cci(chipcard_t cc);
int chipcard_wait_for_card(chipcard_t cc);

#define CHIPCARD_ACTIVE		0x0
#define CHIPCARD_PRESENT	0x1
#define CHIPCARD_NOT_PRESENT	0x2
unsigned int chipcard_status(chipcard_t cc);

#define CHIPCARD_CLOCK_ERR	0x0
#define CHIPCARD_CLOCK_START	0x1
#define CHIPCARD_CLOCK_STOP_L	0x2
#define CHIPCARD_CLOCK_STOP_H	0x3
#define CHIPCARD_CLOCK_STOP	0x4
unsigned int chipcard_slot_status(chipcard_t cc);

#define CHIPCARD_AUTO_VOLTAGE	0x0
#define CHIPCARD_5V		0x1
#define CHIPCARD_3V		0x2
#define CHIPCARD_1_8V		0x3
const uint8_t *chipcard_slot_on(chipcard_t cc, unsigned int voltage,
				size_t *atr_len);
int chipcard_slot_off(chipcard_t cc);

int chipcard_transact(chipcard_t cc, xfr_t xfr);

/* -- Utility functions */
void hex_dump(const uint8_t *ptr, size_t len, size_t llen);
void hex_dumpf(FILE *f, const uint8_t *ptr, size_t len, size_t llen);
void ber_dump(const uint8_t *ptr, size_t len, unsigned int depth);

#endif /* _CCID_H */
