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

#include "compiler.h"
#include "bytesex.h"

#include <usb.h>

typedef struct _cci *cci_t;
typedef struct _chipcard *chipcard_t;

cci_t cci_probe(struct usb_device *dev, int c, int i, int a);
void cci_close(cci_t cci);

#endif /* _CCID_H */
