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

union _rfid_layer3 {
	struct tcl_handle tcl;
};

typedef int (*rfid_l3_t)(struct _cci *cci, struct rfid_tag *tag,
			union _rfid_layer3 *l3p,
			const unsigned char *tx_data, size_t tx_len,
			unsigned char *rx_data, size_t *rx_len);
struct _rfid {
	struct _ccid *rf_ccid;

	const struct rfid_layer1_ops *rf_l1;
	void *rf_l1p;

	struct rfid_tag rf_tag;

	rfid_l3_t rf_l3;
	union _rfid_layer3 rf_l3p;
};

#endif /* RFID_INTERNAL_H */
