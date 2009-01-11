/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/
#ifndef _CCID_INTERNAL_H
#define _CCID_INTERNAL_H

#include <usb.h>
#include <ccid-spec.h>

#define trace(cci, fmt, x...) \
		do { \
			struct _cci *_CCI = cci; \
			if ( _CCI->cci_tf ) \
				fprintf(_CCI->cci_tf, fmt , ##x); \
		}while(0);

struct _chipcard {
	struct _cci *cc_parent;
	uint8_t cc_idx;
	uint8_t cc_status;
};

struct _cci {
	usb_dev_handle *cci_dev;

	struct _xfr	*cci_xfr;

	FILE		*cci_tf;

	int 		cci_inp;
	int 		cci_outp;
	int 		cci_intrp;

	uint16_t 	cci_max_in;
	uint16_t 	cci_max_out;
	uint16_t 	cci_max_intr;

	uint8_t 	cci_seq;
	uint8_t		_pad0;

	unsigned int 	cci_num_slots;
	unsigned int 	cci_max_slots;
	struct _chipcard cci_slot[CCID_MAX_SLOTS];

	struct ccid_desc cci_desc;
};

struct _xfr {
	size_t 		x_txmax, x_rxmax;
	size_t 		x_txlen, x_rxlen;
	struct ccid_msg *x_txhdr;
	uint8_t 	*x_txbuf;
	const struct ccid_msg	*x_rxhdr;
	uint8_t 	*x_rxbuf;
};

int _probe_device(struct usb_device *dev, int *cp, int *ip, int *ap);

int _RDR_to_PC(struct _cci *cci, unsigned int slot, struct _xfr *xfr);
unsigned int _RDR_to_PC_SlotStatus(struct _cci *cci, struct _xfr *xfr);
unsigned int _RDR_to_PC_DataBlock(struct _cci *cci, struct _xfr *xfr);

int _PC_to_RDR_GetSlotStatus(struct _cci *cci, unsigned int slot,
				struct _xfr *xfr);
int _PC_to_RDR_IccPowerOn(struct _cci *cci, unsigned int slot,
				struct _xfr *xfr,
				unsigned int voltage);
int _PC_to_RDR_IccPowerOff(struct _cci *cci, unsigned int slot,
				struct _xfr *xfr);
int _PC_to_RDR_XfrBlock(struct _cci *cci, unsigned int slot, struct _xfr *xfr);

int _cci_wait_for_interrupt(struct _cci *cci);

#endif /* _CCID_INTERNAL_H */
