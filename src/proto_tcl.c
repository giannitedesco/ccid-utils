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
#include "rfid_layer1.h"
#include "iso14443a.h"
#include "proto_tcl.h"

#define RFID_MAX_FRAMELEN	256

enum tcl_pcb_bits {
	TCL_PCB_CID_FOLLOWING		= 0x08,
	TCL_PCB_NAD_FOLLOWING		= 0x04,
};

enum tcl_pcd_state {
	TCL_STATE_NONE = 0x00,
	TCL_STATE_INITIAL,
	TCL_STATE_RATS_SENT,		/* waiting for ATS */
	TCL_STATE_ATS_RCVD,		/* ATS received */
	TCL_STATE_PPS_SENT,		/* waiting for PPS response */
	TCL_STATE_ESTABLISHED,		/* xchg transparent data */
	TCL_STATE_DESELECT_SENT,	/* waiting for DESELECT response */
	TCL_STATE_DESELECTED,		/* card deselected or HLTA'd */
};

enum tcl_handle_flags {
	TCL_HANDLE_F_NAD_SUPPORTED 	= 0x0001,
	TCL_HANDLE_F_CID_SUPPORTED 	= 0x0002,
	TCL_HANDLE_F_NAD_USED		= 0x0010,
	TCL_HANDLE_F_CID_USED		= 0x0020,
};

static enum rfid_frametype l2_to_frame(unsigned int layer2)
{
	switch (layer2) {
		case RFID_LAYER2_ISO14443A:
			return RFID_14443A_FRAME_REGULAR;
			break;
		case RFID_LAYER2_ISO14443B:
			return RFID_14443B_FRAME_REGULAR;
			break;
	}
	return 0;
}

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

	return (tmp / _rfid_layer1_carrier_freq(cci)) * multiplier;
}

static unsigned int fwi_to_fwt(struct _cci *cci,
				unsigned char fwi)
{
	unsigned int multiplier, tmp;

	if (fwi > 14)
		fwi = 14;

	multiplier = 1 << fwi; /* 2 to the power of fwi */

	/* ISO 14443-4:2000(E) Section 7.2.:
	 * (256*16 / h->l2h->rh->ah->fc) * (2 ^ fwi) */

	tmp = (unsigned int) 1000000 * 256 * 16;

	return (tmp / _rfid_layer1_carrier_freq(cci)) * multiplier;
}

#define ATS_TA_DIV_2	1
#define ATS_TA_DIV_4	2
#define ATS_TA_DIV_8	4

#define PPS_DIV_8	3
#define PPS_DIV_4	2
#define PPS_DIV_2	1
#define PPS_DIV_1	0
static unsigned char d_to_di(struct _cci *cci, unsigned char D)
{
	static char DI;
	unsigned int speeds = _rfid_layer1_get_speeds(cci);
	
	if ((D & ATS_TA_DIV_8) && (speeds & (1 << RFID_14443A_SPEED_848K)))
		DI = PPS_DIV_8;
	else if ((D & ATS_TA_DIV_4) && (speeds & (1 << RFID_14443A_SPEED_424K)))
		DI = PPS_DIV_4;
	else if ((D & ATS_TA_DIV_2) && (speeds & (1 <<RFID_14443A_SPEED_212K)))
		DI = PPS_DIV_2;
	else
		DI = PPS_DIV_1;

	return DI;
}

static unsigned int di_to_speed(unsigned char DI)
{
	switch (DI) {
	case PPS_DIV_8:
		return RFID_14443A_SPEED_848K;
	case PPS_DIV_4:
		return RFID_14443A_SPEED_424K;
	case PPS_DIV_2:
		return RFID_14443A_SPEED_212K;
	case PPS_DIV_1:
		return RFID_14443A_SPEED_106K;
	}
	abort();
}

/* start a PPS run (autimatically configure highest possible speed */
static int do_pps(struct _cci *cci, struct rfid_tag *tag,
			struct tcl_handle *h)
{
	unsigned char ppss[3];
	/* FIXME: this stinks like hell. IF we reduce pps_response size to one,
	   we'll get stack corruption! */
	unsigned char pps_response[10];
	unsigned int rx_len = 1;
	unsigned char Dr, Ds, DrI, DsI;
	unsigned int speed;

	if (h->state != TCL_STATE_ATS_RCVD)
		return 0;

	Dr = h->ta & 0x07;
	Ds = h->ta & 0x70 >> 4;
	//printf("Dr = 0x%x, Ds = 0x%x\n", Dr, Ds);

	if (Dr != Ds && !(h->ta & 0x80)) {
		/* device supports different divisors for rx and tx, but not
		 * really ?!? */
		printf("PICC has contradictory TA, aborting PPS\n");
		return 0;
	};

	/* ISO 14443-4:2000(E) Section 5.3. */

	ppss[0] = 0xd0 | (h->cid & 0x0f);
	ppss[1] = 0x11;
	ppss[2] = 0x00;

	/* FIXME: deal with different speed for each direction */
	DrI = d_to_di(cci, Dr);
	DsI = d_to_di(cci, Ds);
	//printf("DrI = 0x%x, DsI = 0x%x\n", DrI, DsI);

	ppss[2] = (ppss[2] & 0xf0) | (DrI | DsI << 2);

	if ( !_iso14443ab_transceive(cci, RFID_14443A_FRAME_REGULAR,
					ppss, 3, pps_response, &rx_len,
					h->fwt) )
		return 0;

	if (pps_response[0] != ppss[0]) {
		printf("PPS Response != PPSS\n");
		return 0;
	}

	speed = di_to_speed(DrI);
	if ( !_rfid_layer1_set_speed(cci, speed) )
		return 0;

	return 1;
}

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
	if (h->fsc > _rfid_layer1_mtu(cci) )
		h->fsc = _rfid_layer1_mtu(cci);

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

	//h->historical_len = (ats+len) - cur;
	//h->historical_bytes = cur;
	
	return 1;
}

struct fr_buff {
	unsigned int frame_len;		/* length of frame */
	unsigned int hdr_len;		/* length of header within frame */
	unsigned char data[RFID_MAX_FRAMELEN];
};

#define frb_payload(x)	(x.data + x.hdr_len)


/* RFID transceive buffer. */
struct rfid_xcvb {
	struct rfix_xcvb *next;		/* next in queue of buffers */

	u_int64_t timeout;		/* timeout to wait for reply */
	struct fr_buff tx;
	struct fr_buff rx;
};

struct tcl_tx_context {
	const unsigned char *tx;
	unsigned char *rx;
	const unsigned char *next_tx_byte;
	unsigned char *next_rx_byte;
	unsigned int rx_len;
	unsigned int tx_len;
	struct rfid_protocol_handle *h;
};

#define tcl_ctx_todo(ctx) (ctx->tx_len - (ctx->next_tx_byte - ctx->tx))
#define is_s_block(x) ((x & 0xc0) == 0xc0)
#define is_r_block(x) ((x & 0xc0) == 0x80)
#define is_i_block(x) ((x & 0xc0) == 0x00)

static int
tcl_build_prologue2(struct tcl_handle *th, 
		    unsigned char *prlg, unsigned int *prlg_len, 
		    unsigned char pcb)
{
	*prlg_len = 1;

	*prlg = pcb;

	if (!is_s_block(pcb)) {
		if (th->toggle) {
			/* we've sent a toggle bit last time */
			th->toggle = 0;
		} else {
			/* we've not sent a toggle last time: send one */
			th->toggle = 1;
			*prlg |= 0x01;
		}
	}

	if (th->flags & TCL_HANDLE_F_CID_USED) {
		/* ISO 14443-4:2000(E) Section 7.1.1.2 */
		*prlg |= TCL_PCB_CID_FOLLOWING;
		(*prlg_len)++;
		prlg[*prlg_len] = th->cid & 0x0f;
	}

	/* nad only for I-block */
	if ((th->flags & TCL_HANDLE_F_NAD_USED) && is_i_block(pcb)) {
		/* ISO 14443-4:2000(E) Section 7.1.1.3 */
		/* FIXME: in case of chaining only for first frame */
		*prlg |= TCL_PCB_NAD_FOLLOWING;
		prlg[*prlg_len] = th->nad;
		(*prlg_len)++;
	}

	return 0;
}

static int
tcl_build_prologue_i(struct tcl_handle *th,
		     unsigned char *prlg, unsigned int *prlg_len)
{
	/* ISO 14443-4:2000(E) Section 7.1.1.1 */
	return tcl_build_prologue2(th, prlg, prlg_len, 0x02);
}

static int
tcl_build_prologue_r(struct tcl_handle *th,
		     unsigned char *prlg, unsigned int *prlg_len,
		     unsigned int nak)
{
	unsigned char pcb = 0xa2;
	/* ISO 14443-4:2000(E) Section 7.1.1.1 */

	if (nak)
		pcb |= 0x10;

	return tcl_build_prologue2(th, prlg, prlg_len, pcb);
}

static int
tcl_build_prologue_s(struct tcl_handle *th,
		     unsigned char *prlg, unsigned int *prlg_len)
{
	/* ISO 14443-4:2000(E) Section 7.1.1.1 */

	/* the only S-block from PCD->PICC is DESELECT,
	 * well, actually there is the S(WTX) response. */
	return tcl_build_prologue2(th, prlg, prlg_len, 0xc2);
}

/* FIXME: WTXM implementation */

static int tcl_prlg_len(struct tcl_handle *th)
{
	int prlg_len = 1;

	if (th->flags & TCL_HANDLE_F_CID_USED)
		prlg_len++;

	if (th->flags & TCL_HANDLE_F_NAD_USED)
		prlg_len++;

	return prlg_len;
}

#define max_net_tx_framesize(x)	(x->fsc - tcl_prlg_len(x))

static int 
tcl_refill_xcvb(struct tcl_handle *th, struct rfid_xcvb *xcvb,
		struct tcl_tx_context *ctx)
{
	if (ctx->next_tx_byte >= ctx->tx + ctx->tx_len) {
		printf("tyring to refill tx xcvb but no data left!\n");
		return -1;
	}

	if (tcl_build_prologue_i(th, xcvb->tx.data, 
				 &xcvb->tx.hdr_len) < 0)
		return -1;

	if (tcl_ctx_todo(ctx) > th->fsc - xcvb->tx.hdr_len)
		xcvb->tx.frame_len = max_net_tx_framesize(th);
	else
		xcvb->tx.frame_len = tcl_ctx_todo(ctx);

	memcpy(frb_payload(xcvb->tx), ctx->next_tx_byte,
		xcvb->tx.frame_len);

	ctx->next_tx_byte += xcvb->tx.frame_len;

	/* check whether we need to set the chaining bit */
	if (ctx->next_tx_byte < ctx->tx + ctx->tx_len)
		xcvb->tx.data[0] |= 0x10;

	/* add hdr_len after copying the net payload */
	xcvb->tx.frame_len += xcvb->tx.hdr_len;

	xcvb->timeout = th->fwt;

	return 0;
}

static void fill_xcvb_wtxm(struct tcl_handle *th, struct rfid_xcvb *xcvb,
			unsigned char inf)
{
	/* Acknowledge WTXM */
	tcl_build_prologue_s(th, xcvb->tx.data, &xcvb->tx.hdr_len);
	/* set two bits that make this block a wtx */
	xcvb->tx.data[0] |= 0x30;
	xcvb->tx.data[xcvb->tx.hdr_len] = inf;
	xcvb->tx.frame_len = xcvb->tx.hdr_len+1;
	xcvb->timeout = th->fwt * inf;
}

static int check_cid(struct tcl_handle *th, struct rfid_xcvb *xcvb)
{
	if (xcvb->rx.data[0] & TCL_PCB_CID_FOLLOWING) {
		if (xcvb->rx.data[1] != th->cid) {
			printf("CID %u is not valid, we expected %u\n", 
				xcvb->rx.data[1], th->cid);
			return 0;
		}
	}
	return 1;
}

int _tcl_transceive(struct _cci *cci, struct rfid_tag *tag,
		struct tcl_handle *th,
		const unsigned char *tx_data, unsigned int tx_len,
		unsigned char *rx_data, unsigned int *rx_len,
		unsigned int timeout)
{
	int ret;

	struct rfid_xcvb xcvb;
	struct tcl_tx_context tcl_ctx;

	unsigned char ack[10];
	unsigned int ack_len;

	/* initialize context */
	tcl_ctx.next_tx_byte = tcl_ctx.tx = tx_data;
	tcl_ctx.next_rx_byte = tcl_ctx.rx = rx_data;
	tcl_ctx.rx_len = *rx_len;
	tcl_ctx.tx_len = tx_len;

	/* initialize xcvb */
	xcvb.timeout = th->fwt;

tx_refill:
	if (tcl_refill_xcvb(th, &xcvb, &tcl_ctx) < 0) {
		ret = -1;
		goto out;
	}

do_tx:
	xcvb.rx.frame_len = sizeof(xcvb.rx.data);
	if ( !_iso14443ab_transceive(cci, l2_to_frame(tag->layer2),
				     xcvb.tx.data, xcvb.tx.frame_len,
				     xcvb.rx.data, &xcvb.rx.frame_len,
				     xcvb.timeout) )
		return 0;
	printf("l2 transceive finished\n");

	if (is_r_block(xcvb.rx.data[0])) {
		printf("R-Block\n");

		if ((xcvb.rx.data[0] & 0x01) != th->toggle) {
			printf("response with wrong toggle bit\n");
			goto out;
		}

		/* Handle ACK frame in case of chaining */
		if (!check_cid(th, &xcvb))
			goto out;

		goto tx_refill;
	} else if (is_s_block(xcvb.rx.data[0])) {
		unsigned char inf;

		printf("S-Block\n");
		/* Handle Wait Time Extension */
		
		if (!check_cid(th, &xcvb))
			goto out;

		if (xcvb.rx.data[0] & TCL_PCB_CID_FOLLOWING) {
			if (xcvb.rx.frame_len < 3) {
				printf("S-Block with CID but short len\n");
				ret = -1;
				goto out;
			}
			inf = xcvb.rx.data[2];
		} else
			inf = xcvb.rx.data[1];

		if ((xcvb.rx.data[0] & 0x30) != 0x30) {
			printf("S-Block but not WTX?\n");
			ret = -1;
			goto out;
		}
		inf &= 0x3f;	/* only lower 6 bits code WTXM */
		if (inf == 0 || (inf >= 60 && inf <= 63)) {
			printf("WTXM %u is RFU!\n", inf);
			ret = -1;
			goto out;
		}
		
		fill_xcvb_wtxm(th, &xcvb, inf);
		/* start over with next transceive */
		goto do_tx; 
	} else if (is_i_block(xcvb.rx.data[0])) {
		unsigned int net_payload_len;
		/* we're actually receiving payload data */

		printf("I-Block: ");

		if ((xcvb.rx.data[0] & 0x01) != th->toggle) {
			printf("response with wrong toggle bit\n");
			goto out;
		}

		xcvb.rx.hdr_len = 1;

		if (!check_cid(th, &xcvb))
			goto out;

		if (xcvb.rx.data[0] & TCL_PCB_CID_FOLLOWING)
			xcvb.rx.hdr_len++;
		if (xcvb.rx.data[0] & TCL_PCB_NAD_FOLLOWING)
			xcvb.rx.hdr_len++;
	
		net_payload_len = xcvb.rx.frame_len - xcvb.rx.hdr_len;
		printf("%u bytes\n", net_payload_len);
		memcpy(tcl_ctx.next_rx_byte, &xcvb.rx.data[xcvb.rx.hdr_len], 
			net_payload_len);
		tcl_ctx.next_rx_byte += net_payload_len;

		if (xcvb.rx.data[0] & 0x10) {
			/* we're not the last frame in the chain, continue rx */
			printf("not the last frame in the chain, continue\n");
			ack_len = sizeof(ack);
			tcl_build_prologue_r(th, xcvb.tx.data, &xcvb.tx.frame_len, 0);
			xcvb.timeout = th->fwt;
			goto do_tx;
		}
	}

out:
	*rx_len = tcl_ctx.next_rx_byte - tcl_ctx.rx;
	return ret;
}

#define CID	0
#define TIMEOUT	(((uint64_t)1000000 * 65536 / ISO14443_FREQ_CARRIER))
int _tcl_get_ats(struct _cci *cci, struct rfid_tag *tag,
		 struct tcl_handle *th)
{
	struct _ccid *ccid;
	uint8_t ats[64];
	uint8_t rats[2];
	size_t ats_len;
	uint8_t fsdi;

	memset(th, 0, sizeof(*th));
	th->toggle = 1;

	_iso14443_fsd_to_fsdi(_rfid_layer1_mtu(cci), &fsdi);
	rats[0] = 0xe0;
	rats[1] = (CID & 0xf) | ((fsdi & 0xf) << 4);

	ats_len = sizeof(ats);
	if ( !_iso14443ab_transceive(cci, RFID_14443A_FRAME_REGULAR,
				rats, sizeof(rats),
				ats, &ats_len,
				TIMEOUT) )
		return 0;

	th->state = TCL_STATE_ATS_RCVD;
	ccid = cci->cc_parent;

	if ( !parse_ats(cci, tag, th, ats, ats_len) ) {
		return 0;
	}

	if ( !do_pps(cci, tag, th) )
		return 0;

	memcpy(ccid->cci_xfr->x_rxbuf, ats, ats_len);
	ccid->cci_xfr->x_rxlen = ats_len;
	return 1;
}
