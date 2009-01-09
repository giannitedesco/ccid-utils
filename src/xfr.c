/*
 * This file is part of cci-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>
#include <ccid-spec.h>

#include <stdio.h>

#include "ccid-internal.h"

static size_t x_rbuflen(struct _xfr *xfr)
{
	return xfr->x_rxmax + sizeof(struct ccid_msg);
}
static size_t x_tbuflen(struct _xfr *xfr)
{
	return xfr->x_txlen + sizeof(struct ccid_msg);
}

int _cci_wait_for_interrupt(struct _cci *cci)
{
	uint8_t buf[cci->cci_max_intr];
	int ret;
	size_t len;

	ret = usb_interrupt_read(cci->cci_dev, cci->cci_intrp,
				(void *)buf, cci->cci_max_intr, 0);
	if ( ret < 0 )
		return 0;

	len = (size_t)ret;

	printf(" Intr: %u byte interrupt packet\n", len);
	if ( len < 1 )
		return 1;

	switch( buf[0] ) {
	case RDR_to_PC_NotifySlotChange:
		assert(2 == len);
		if ( buf[1] & 0x2 ) {
			printf("     : Slot status changed to %s\n",
				((buf[1] & 0x1) == 0) ? 
					"NOT present" : "present");
			cci->cci_slot[0].cc_status =
				((buf[1] & 0x1) == 0) ?
					CHIPCARD_NOT_PRESENT :
					CHIPCARD_PRESENT;
		}
		break;
	case RDR_to_PC_HardwareError:
		printf("     : HALT AND CATCH FIRE!!\n");
		break;
	default:
		fprintf(stderr, "*** error: unknown interrupt packet\n");
		break;
	}

	return 1;
}

unsigned int _RDR_to_PC_DataBlock(struct _cci *cci, struct _xfr *xfr)
{
	assert(xfr->x_rxhdr->bMessageType == RDR_to_PC_DataBlock);
	printf("     : RDR_to_PC_DataBlock: %u bytes\n", xfr->x_rxlen);
	hex_dump(xfr->x_rxbuf, xfr->x_rxlen, 16);
	return xfr->x_rxhdr->in.bApp; /* APDU chaining parameter */
}

unsigned int _RDR_to_PC_SlotStatus(struct _cci *cci, struct _xfr *xfr)
{
	assert(xfr->x_rxhdr->bMessageType == RDR_to_PC_SlotStatus);

	printf("     : RDR_to_PC_SlotStatus: ");
	switch( xfr->x_rxhdr->in.bApp ) {
	case 0x00:
		printf("Clock running\n");
		return CHIPCARD_CLOCK_START;
	case 0x01:
		printf("Clock stopped in L state\n");
		return CHIPCARD_CLOCK_STOP_L;
	case 0x02:
		printf("Clock stopped in H state\n");
		return CHIPCARD_CLOCK_STOP_H;
	case 0x03:
		printf("Clock stopped in unknown state\n");
		return CHIPCARD_CLOCK_STOP;
	default:
		return CHIPCARD_CLOCK_ERR;
	}
}

static int _cmd_result(const struct ccid_msg *msg)
{
	switch( msg->in.bStatus & CCID_STATUS_RESULT_MASK ) {
	case CCID_RESULT_SUCCESS:
		printf("     : Command: SUCCESS\n");
		return 1;
	case CCID_RESULT_ERROR:
		printf("     : Command: FAILED (%d)\n", msg->in.bError);
		return 0;
	case CCID_RESULT_TIMEOUT:
		printf("     : Command: Time Extension Request\n");
		return 0;
	default:
		fprintf(stderr, "*** error: unknown command result\n");
		return 0;
	}
}

static int do_recv(struct _cci *cci, struct _xfr *xfr)
{
	int ret;
	size_t len;

	ret = usb_bulk_read(cci->cci_dev, cci->cci_inp,
				(void *)xfr->x_rxhdr,
				x_rbuflen(xfr), 0);
	if ( ret < 0 ) {
		fprintf(stderr, "*** error: usb_bulk_read()\n");
		return 0;
	}

	len = (size_t)ret;

	if ( len < sizeof(*xfr->x_rxhdr) ) {
		fprintf(stderr, "*** error: truncated CCI msg\n");
		return 0;
	}

	if ( sizeof(*xfr->x_rxhdr) + sys_le32(xfr->x_rxhdr->dwLength) > len ) {
		fprintf(stderr, "*** error: bad dwLength in CCI msg\n");
		return 0;
	}

	xfr->x_rxlen = sys_le32(xfr->x_rxhdr->dwLength);

	return 1;
}

int _RDR_to_PC(struct _cci *cci, unsigned int slot, struct _xfr *xfr)
{
	const struct ccid_msg *msg;

	if ( !do_recv(cci, xfr) )
		return 0;

	msg = xfr->x_rxhdr;

	printf(" Recv: %d bytes for slot %u (seq = 0x%.2x)\n",
		xfr->x_rxlen, msg->bSlot, msg->bSeq);

	if ( msg->bSlot != slot ) {
		fprintf(stderr, "*** error: bad slot %u (expected %u)\n",
			msg->bSlot, slot);
		return 0;
	}

	if ( (uint8_t)(msg->bSeq + 1) != cci->cci_seq ) {
		fprintf(stderr, "*** error: expected seq 0x%.2x got 0x%.2x\n",
			cci->cci_seq, (uint8_t)(msg->bSeq + 1));
		return 0;
	}

	_chipcard_set_status(&cci->cci_slot[msg->bSlot], msg->in.bStatus);

	return _cmd_result(xfr->x_rxhdr);
}

static int _PC_to_RDR(struct _cci *cci, unsigned int slot, struct _xfr *xfr)
{
	int ret;
	size_t len;

	assert(slot < cci->cci_num_slots);

	xfr->x_txhdr->dwLength = sys_le32(xfr->x_txlen);
	xfr->x_txhdr->bSlot = slot;
	xfr->x_txhdr->bSeq = cci->cci_seq++;

	ret = usb_bulk_write(cci->cci_dev, cci->cci_outp,
				(void *)xfr->x_txhdr, x_tbuflen(xfr), 0);
	if ( ret < 0 ) {
		fprintf(stderr, "*** error: usb_bulk_read()\n");
		return 0;
	}

	len = (size_t)ret;

	if ( ret < x_tbuflen(xfr) ) {
		fprintf(stderr, "*** error: truncated TX: %u/%u\n",
			len, x_tbuflen(xfr));
		return 0;
	}

	return 1;
}

int _PC_to_RDR_XfrBlock(struct _cci *cci, unsigned int slot, struct _xfr *xfr)
{
	int ret;

	memset(xfr->x_txhdr, 0, sizeof(*xfr->x_txhdr));
	xfr->x_txhdr->bMessageType = PC_to_RDR_XfrBlock;
	ret = _PC_to_RDR(cci, slot, xfr);
	if ( ret ) {
		printf(" Xmit: PC_to_RDR_XfrBlock(%u)\n", slot);
		hex_dump(xfr->x_txbuf, xfr->x_txlen, 16);
	}
	return ret;
}

int _PC_to_RDR_GetSlotStatus(struct _cci *cci, unsigned int slot,
				struct _xfr *xfr)
{
	int ret;

	memset(xfr->x_txhdr, 0, sizeof(*xfr->x_txhdr));
	xfr->x_txhdr->bMessageType = PC_to_RDR_GetSlotStatus;
	ret = _PC_to_RDR(cci, slot, xfr);
	if ( ret )
		printf(" Xmit: PC_to_RDR_GetSlotStatus(%u)\n", slot);

	return ret;
}

int _PC_to_RDR_IccPowerOn(struct _cci *cci, unsigned int slot,
				struct _xfr *xfr,
				unsigned int voltage)
{
	int ret;

	assert(voltage <= CHIPCARD_1_8V);

	memset(xfr->x_txhdr, 0, sizeof(*xfr->x_txhdr));
	xfr->x_txhdr->bMessageType = PC_to_RDR_IccPowerOn;
	xfr->x_txhdr->out.bApp[0] = voltage & 0xff;
	ret = _PC_to_RDR(cci, slot, xfr);
	if ( ret ) {
		printf(" Xmit: PC_to_RDR_IccPowerOn(%u)\n", slot);
		switch(voltage) {
		case CHIPCARD_AUTO_VOLTAGE:
			printf("     : Automatic Voltage Selection\n");
			break;
		case CHIPCARD_5V:
			printf("     : 5 Volts\n");
			break;
		case CHIPCARD_3V:
			printf("     : 3 Volts\n");
			break;
		case CHIPCARD_1_8V:
			printf("     : 1.8 Volts\n");
			break;
		}
	}

	return ret;
}

int _PC_to_RDR_IccPowerOff(struct _cci *cci, unsigned int slot,
				struct _xfr *xfr)
{
	int ret;

	memset(xfr->x_txhdr, 0, sizeof(*xfr->x_txhdr));
	xfr->x_txhdr->bMessageType = PC_to_RDR_IccPowerOff;
	ret = _PC_to_RDR(cci, slot, xfr);
	if ( ret ) {
		printf(" Xmit: PC_to_RDR_IccPowerOff(%u)\n", slot);
	}

	return ret;
}

xfr_t  xfr_alloc(size_t txbuf, size_t rxbuf)
{
	uint8_t *ptr;
	struct _xfr *xfr;
	size_t tot_len;

	tot_len = sizeof(*xfr) + txbuf + rxbuf + sizeof(*xfr->x_txhdr) * 2;
	ptr = calloc(1, tot_len);
	if ( NULL == ptr )
		return NULL;
	
	xfr = (struct _xfr *)ptr;
	ptr += sizeof(*xfr);

	xfr->x_txmax = txbuf;
	xfr->x_rxmax = txbuf;

	xfr->x_txhdr = (struct ccid_msg *)ptr;
	ptr += sizeof(*xfr->x_txhdr);

	xfr->x_txbuf = ptr;
	ptr += txbuf;

	xfr->x_rxhdr = (struct ccid_msg *)ptr;
	ptr += sizeof(*xfr->x_rxhdr);

	xfr->x_rxbuf = ptr;

	return xfr;
}

void xfr_reset(xfr_t xfr)
{
	xfr->x_txlen = xfr->x_rxlen = 0;
	xfr->x_txptr = xfr->x_txbuf;
}

int xfr_tx_byte(xfr_t xfr, uint8_t byte)
{
	uint8_t *end;

	end = xfr->x_txbuf + xfr->x_txmax;
	if ( xfr->x_txptr >= end )
		return 0;

	*xfr->x_txptr = byte;
	xfr->x_txptr++;
	xfr->x_txlen++;

	return 1;
}

int xfr_tx_buf(xfr_t xfr, const uint8_t *ptr, size_t len)
{
	uint8_t *end;

	end = xfr->x_txbuf + xfr->x_txmax;
	if ( xfr->x_txptr + len > end )
		return 0;

	memcpy(xfr->x_txptr, ptr, len);
	xfr->x_txptr += len;
	xfr->x_txlen += len;

	return 1;
}

uint8_t xfr_rx_sw1(xfr_t xfr)
{
	uint8_t *end;
	assert(xfr->x_rxlen >= 2U);
	end = xfr->x_rxbuf + xfr->x_rxlen;
	return end[-2];
}

uint8_t xfr_rx_sw2(xfr_t xfr)
{
	uint8_t *end;
	assert(xfr->x_rxlen >= 2U);
	end = xfr->x_rxbuf + xfr->x_rxlen;
	return end[-1];
}

const uint8_t *xfr_rx_data(xfr_t xfr, size_t *len)
{
	if ( xfr->x_rxlen < 2 )
		return NULL;
	
	*len = xfr->x_rxlen - 2;
	return xfr->x_rxbuf;
}

void xfr_free(xfr_t xfr)
{
	free(xfr);
}
