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

struct _clrc632_ops {
	int (*fifo_read)(struct _ccid *ccid, unsigned int field,
			 uint8_t *buf, size_t len);
	int (*fifo_write)(struct _ccid *ccid, unsigned int field,
			  const uint8_t *buf, size_t len);
	int (*reg_read)(struct _ccid *ccid, unsigned int field,
			unsigned int reg, uint8_t *val);
	int (*reg_write)(struct _ccid *ccid, unsigned int field,
			 unsigned int reg, uint8_t val);
};

struct _cci {
	struct _ccid *cc_parent;
	uint8_t cc_idx;
	uint8_t cc_status;

	/* Fields related to proprietary interfaces */
	const struct _clrc632_ops *cc_rc632;
};

#define RFID_MAX_FIELDS 1

struct _ccid {
	libusb_device_handle *cci_dev;

	struct _xfr	*cci_xfr;

	FILE		*cci_tf;

	/* USB interfacee */
	int 		cci_inp;
	int 		cci_outp;
	int 		cci_intrp;

	uint16_t 	cci_max_in;
	uint16_t 	cci_max_out;
	uint16_t 	cci_max_intr;

	uint8_t 	cci_seq;
	uint8_t		_pad0;

	/* cci slots */
	unsigned int 	cci_num_slots;
	unsigned int 	cci_max_slots;
	struct _cci cci_slot[CCID_MAX_SLOTS];

	/* RF fields */
	unsigned int	cci_num_rf;
	struct _cci cci_rf[RFID_MAX_FIELDS];

	/* CCID USB descriptor */
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

#define INTF_RFID_OMNI	(1<<0)
struct _cci_interface {
	int c, i, a;
	const char *name;
	unsigned int flags;
};

_private void _omnikey_init_prox(struct _ccid *ccid);
_private int _clrc632_init(struct _cci *cc);

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
