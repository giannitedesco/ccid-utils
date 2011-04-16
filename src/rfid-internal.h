/*
 * This file is part of ccid-utils
 * Copyright (c) 2011 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/
#ifndef _RFID_INTERNAL_H
#define _RFID_INTERNAL_H

#include "rfid.h"
#include "rfid_layer1.h"
#include "proto_tcl.h"

struct _rfid {
	struct _ccid *rf_ccid;

	const struct rfid_layer1_ops *rf_l1;
	void *rf_l1p;

	struct rfid_tag rf_tag;

	union {
		struct tcl_handle tcl;
	}rf_l2;
};

#endif /* RFID_INTERNAL_H */
