/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/
#ifndef _CCID_INTERNAL_H
#define _CCID_INTERNAL_H

#include <libusb.h>
#include <ccid-spec.h>

#define trace(ccid, fmt, x...) \
		do { \
			struct _ccid *_CCI = ccid; \
			if ( _CCI->cci_tf ) { \
				fprintf(_CCI->cci_tf, fmt , ##x); \
				fflush(_CCI->cci_tf); \
			} \
		}while(0);

struct _cci_ops {
	const uint8_t *(*power_on)(struct _cci *cci,
					unsigned int v,
					size_t *atr_len);
	int (*power_off)(struct _cci *cci);
	int (*transact)(struct _cci *cc, struct _xfr *xfr);
	void (*dtor)(struct _cci *cc);
};
extern const struct _cci_ops _contact_ops;
extern const struct _cci_ops _rfid_ops;

struct _cci {
	struct _ccid *cc_parent;
	uint8_t cc_idx;
	uint8_t cc_status;
	const struct _cci_ops *cc_ops;
	void *cc_priv;
};

#define RFID_MAX_FIELDS 1

struct _ccid {
	libusb_device_handle *cci_dev;

	struct _xfr	*cci_xfr;

	FILE		*cci_tf;

	/* USB interface */
	int 		cci_inp;
	int 		cci_outp;
	int 		cci_intrp;

	uint16_t 	cci_max_in;
	uint16_t 	cci_max_out;
	uint16_t 	cci_max_intr;

	uint8_t 	cci_seq;
	uint8_t		cci_bus, cci_addr;

	/* cci slots */
	unsigned int 	cci_num_slots;
	unsigned int 	cci_max_slots;
	struct _cci cci_slot[CCID_MAX_SLOTS];

	/* RF fields */
	unsigned int	cci_num_rf;
	struct _cci cci_rf[RFID_MAX_FIELDS];

	/* CCID USB descriptor */
	struct ccid_desc cci_desc;

	char		*cci_name;
};

struct _xfr {
	size_t 		x_txmax, x_rxmax;
	size_t 		x_txlen, x_rxlen;
	struct ccid_msg *x_txhdr;
	uint8_t 	*x_txbuf;
	const struct ccid_msg	*x_rxhdr;
	uint8_t 	*x_rxbuf;
};

#define INTF_RFID_OMNI	(1<<0)
struct _cci_interface {
	int c, i, a;
	const char *name;
	unsigned int flags;
};

_private void _omnikey_init_prox(struct _ccid *ccid);

_private int _probe_descriptors(struct libusb_device *dev,
				struct _cci_interface *intf);

_private int _RDR_to_PC(struct _ccid *ccid, unsigned int slot,
			struct _xfr *xfr);
_private unsigned int _RDR_to_PC_SlotStatus(struct _ccid *ccid,
						struct _xfr *xfr);
_private unsigned int _RDR_to_PC_DataBlock(struct _ccid *ccid,
						struct _xfr *xfr);

_private int _PC_to_RDR_GetSlotStatus(struct _ccid *ccid, unsigned int slot,
					struct _xfr *xfr);
_private int _PC_to_RDR_IccPowerOn(struct _ccid *ccid, unsigned int slot,
					struct _xfr *xfr,
					unsigned int voltage);
_private int _PC_to_RDR_IccPowerOff(struct _ccid *ccid, unsigned int slot,
					struct _xfr *xfr);
_private int _PC_to_RDR_XfrBlock(struct _ccid *ccid, unsigned int slot,
					struct _xfr *xfr);
_private int _PC_to_RDR_Escape(struct _ccid *ccid, unsigned int slot,
					struct _xfr *xfr);

_private int _cci_wait_for_interrupt(struct _ccid *ccid);

_private struct _xfr *_xfr_do_alloc(size_t txbuf, size_t rxbuf);
_private void _xfr_do_free(struct _xfr *xfr);

_private void _hex_dumpf(FILE *f, const uint8_t *tmp, size_t len, size_t llen);

#endif /* _CCID_INTERNAL_H */
