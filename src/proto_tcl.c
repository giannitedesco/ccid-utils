/*
 * This file is part of ccid-utils
 * Copyright (c) 2011 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
 *
 * T=CL layer 3 protocol
 *
 * Much logic liberally copied from librfid
 * (C) 2005-2008 Harald Welte <laforge@gnumonks.org>
*/
#include <ccid.h>
#include <unistd.h>

#include "ccid-internal.h"
#include "rfid.h"
#include "iso14443a.h"
#include "proto_tcl.h"

struct tcl_handle {
	/* derived from ats */
	unsigned char *historical_bytes; /* points into ats */
	unsigned int historical_len;

	unsigned int fsc;	/* max frame size accepted by card */
	unsigned int fsd;	/* max frame size accepted by reader */
	unsigned int fwt;	/* frame waiting time (in usec)*/
	unsigned char ta;	/* divisor information */
	unsigned char sfgt;	/* start-up frame guard time (in usec) */

	/* otherwise determined */
	unsigned int cid;	/* Card ID */
	unsigned int nad;	/* Node Address */

	unsigned int flags;
	unsigned int state;	/* protocol state */

	unsigned int toggle;	/* send toggle with next frame */
};

enum tcl_handle_flags {
	TCL_HANDLE_F_NAD_SUPPORTED 	= 0x0001,
	TCL_HANDLE_F_CID_SUPPORTED 	= 0x0002,
	TCL_HANDLE_F_NAD_USED		= 0x0010,
	TCL_HANDLE_F_CID_USED		= 0x0020,
};

static unsigned int sfgi_to_sfgt(struct _cci *cci,
				 unsigned char sfgi)
{
	unsigned int multiplier;
	unsigned int tmp;

	if (sfgi > 14)
		sfgi = 14;

	multiplier = 1 << sfgi; /* 2 to the power of sfgi */

	/* ISO 14443-4:2000(E) Section 5.2.5:
	 * (256 * 16 / h->l2h->rh->ah->fc) * (2 ^ sfgi) */
	tmp = (unsigned int) 1000000 * 256 * 16;

	return (tmp / _clrc632_carrier_freq(cci)) * multiplier;
}

static unsigned int fwi_to_fwt(struct _cci *cci,
				unsigned char fwi)
{
	unsigned int multiplier, tmp;

	if (fwi > 14)
		fwi = 14;

	multiplier  = 1 << fwi; /* 2 to the power of fwi */

	/* ISO 14443-4:2000(E) Section 7.2.:
	 * (256*16 / h->l2h->rh->ah->fc) * (2 ^ fwi) */

	tmp = (unsigned int) 1000000 * 256 * 16;

	return (tmp / _clrc632_carrier_freq(cci)) * multiplier;
}

/* 4.9seconds as microseconds (4.9 billion seconds) exceeds 2^32 */
#define activation_fwt(x) (((u_int64_t)1000000 * 65536 / x->l2h->rh->ah->fc))
#define deactivation_fwt(x) activation_fwt(x)

static int parse_ats(struct _cci *cci, struct rfid_tag *tag,
			struct tcl_handle *h,
			uint8_t *ats, size_t size)
{
	uint8_t len = ats[0];
	uint8_t t0;
	uint8_t *cur;

	if (len == 0 || size == 0) 
		return 0;

	if (size < len)
		len = size;

	h->ta = 0;

	if (len == 1) {
		/* FIXME: assume some default values */
		h->fsc = 32;
		h->ta = 0x80;	/* 0x80 (same d for both dirs) */
		h->sfgt = sfgi_to_sfgt(cci, 0);
		if (tag->layer2 == RFID_LAYER2_ISO14443A) {
			/* Section 7.2: fwi default for type A is 4 */
			h->fwt = fwi_to_fwt(cci, 4);
		} else {
			/* Section 7.2: fwi for type B is always in ATQB */
			/* Value is assigned in tcl_connect() */
			/* This function is never called for Type B, 
			 * since Type B has no (R)ATS */
		}
		return 1;
	}

	/* guarateed to be at least 2 bytes in size */

	t0 = ats[1];
	cur = &ats[2];

	_iso14443_fsdi_to_fsd(t0 & 0xf, &h->fsc);
	if (h->fsc > _clrc632_mtu(cci) )
		h->fsc =  _clrc632_mtu(cci);

	if (t0 & (1 << 4)) {
		/* TA is transmitted */
		h->ta = *cur++;
	}

	if (t0 & (1 << 5)) {
		/* TB is transmitted */
		h->sfgt = sfgi_to_sfgt(cci, *cur & 0x0f);
		h->fwt = fwi_to_fwt(cci, (*cur & 0xf0) >> 4);
		cur++;
	}

	if (t0 & (1 << 6)) {
		/* TC is transmitted */
		if (*cur & 0x01) {
			h->flags |= TCL_HANDLE_F_NAD_SUPPORTED;
			printf("This PICC supports NAD\n");
		}
		if (*cur & 0x02) {
			h->flags |= TCL_HANDLE_F_CID_SUPPORTED;
			printf("This PICC supports CID\n");
		}
		cur++;
	}

	h->historical_len = (ats+len) - cur;
	h->historical_bytes = cur;
	
	return 1;
}

#define FSD	5 /* 64 bytes */
#define CID	0
#define TIMEOUT	(((uint64_t)1000000 * 65536 / ISO14443_FREQ_CARRIER))
int _tcl_get_ats(struct _cci *cci, struct rfid_tag *tag)
{
	struct tcl_handle tclh;
	struct _ccid *ccid;
	uint8_t ats[64];
	uint8_t rats[2];
	size_t ats_len;
	uint8_t fsdi;

	memset(&tclh, 0, sizeof(tclh));
	tclh.toggle = 1;

	_iso14443_fsd_to_fsdi(_clrc632_mtu(cci), &fsdi);
	rats[0] = 0xe0;
	rats[1] = (CID & 0xf) | ((fsdi & 0xf) << 4);

	ats_len = sizeof(ats);
	if ( !_iso14443ab_transceive(cci, RFID_14443A_FRAME_REGULAR,
				rats, sizeof(rats),
				ats, &ats_len,
				TIMEOUT) )
		return 0;

	ccid = cci->cc_parent;

	if ( !parse_ats(cci, tag, &tclh, ats, ats_len) ) {
		return 0;
	}

	memcpy(ccid->cci_xfr->x_rxbuf, ats, ats_len);
	ccid->cci_xfr->x_rxlen = ats_len;
	return 1;
}
