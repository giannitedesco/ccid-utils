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
#include <assert.h>

#include "compiler.h"
#include "bytesex.h"

#include <usb.h>

typedef struct _cci *cci_t;
typedef struct _chipcard *chipcard_t;

/* Chipcard interface */
cci_t cci_probe(struct usb_device *dev, int c, int i, int a);
unsigned int cci_slots(cci_t cci);
chipcard_t cci_get_slot(cci_t cci, unsigned int i);
void cci_close(cci_t cci);

/* Chipcard */
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
int chipcard_slot_on(chipcard_t cc, unsigned int voltage);
int chipcard_slot_off(chipcard_t cc);

int chipcard_transmit(chipcard_t cc, const uint8_t *data, size_t len);
const uint8_t *chipcard_rcvbuf(chipcard_t cc, size_t *len);

/* Utility functions */
void hex_dump(const void *t, size_t len, size_t llen);

#endif /* _CCID_H */
