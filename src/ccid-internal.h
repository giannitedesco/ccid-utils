/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/
#ifndef _CCID_INTERNAL_H
#define _CCID_INTERNAL_H

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <assert.h>
#if HAVE_ENDIAN_H
#include <endian.h>
#endif

#include <libusb.h>
#include <ccid-spec.h>

#define trace(ccid, fmt, x...) \
		do { \
			struct _ccid *_CCI = ccid; \
			if ( _CCI->d_tf ) { \
				fprintf(_CCI->d_tf, fmt , ##x); \
				fflush(_CCI->d_tf); \
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
	struct _ccid *i_parent;
	uint8_t i_idx;
	uint8_t i_status;
	const struct _cci_ops *i_ops;
	void *i_priv;
};

#define RFID_MAX_FIELDS 1

struct _ccid {
	libusb_device_handle *d_dev;

	struct _xfr	*d_xfr;

	FILE		*d_tf;

	/* USB interface */
	int 		d_inp;
	int 		d_outp;
	int 		d_intrp;

	uint16_t 	d_max_in;
	uint16_t 	d_max_out;
	uint16_t 	d_max_intr;
	uint16_t	d_intf;

	uint8_t 	d_seq;
	uint8_t		d_bus, d_addr;

	/* cci slots */
	unsigned int 	d_num_slots;
	unsigned int 	d_max_slots;
	struct _cci d_slot[CCID_MAX_SLOTS];

	/* RF fields */
	unsigned int	d_num_rf;
	struct _cci d_rf[RFID_MAX_FIELDS];

	/* CCID USB descriptor */
	struct ccid_desc d_desc;

	char		*d_name;
	uint32_t	*d_clock_freq;
	uint32_t	*d_data_rate;
	size_t		d_num_clock;
	size_t		d_num_rate;
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
_private int _RDR_to_PC_Parameters(struct _ccid *ccid, struct _xfr *xfr);

_private int _PC_to_RDR_GetSlotStatus(struct _ccid *ccid, unsigned int slot,
					struct _xfr *xfr);
_private int _PC_to_RDR_GetParameters(struct _ccid *ccid, unsigned int slot,
					struct _xfr *xfr);
_private int _PC_to_RDR_SetParameters(struct _ccid *ccid, unsigned int slot,
					struct _xfr *xfr);
_private int _PC_to_RDR_ResetParameters(struct _ccid *ccid, unsigned int slot,
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
