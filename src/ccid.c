/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
 *
 * Chip Card Interface Device API. Contains USB CCID driver.
*/

#include <ccid.h>

#include <stdarg.h>
#include <inttypes.h>

#include "ccid-internal.h"

static size_t x_rbuflen(struct _xfr *xfr)
{
	return xfr->x_rxmax + sizeof(struct ccid_msg);
}
static size_t x_tbuflen(struct _xfr *xfr)
{
	return xfr->x_txlen + sizeof(struct ccid_msg);
}

int _cci_wait_for_interrupt(struct _ccid *ccid)
{
	const uint8_t *buf;
	int ret;
	size_t len;

	if ( libusb_interrupt_transfer(ccid->cci_dev, ccid->cci_intrp,
				(void *)ccid->cci_xfr->x_rxhdr,
				x_rbuflen(ccid->cci_xfr), &ret, 250) )
		return 0;

	if ( ret == 0 )
		return 1;

	buf = (void *)ccid->cci_xfr->x_rxhdr;
	len = (size_t)ret;

	trace(ccid, " Intr: %zu byte interrupt packet\n", len);
	if ( len < 1 )
		return 1;

	switch( buf[0] ) {
	case RDR_to_PC_NotifySlotChange:
		assert(2 == len);
		if ( buf[1] & 0x2 ) {
			trace(ccid, "     : Slot status changed to %s\n",
				((buf[1] & 0x1) == 0) ? 
					"NOT present" : "present");
			ccid->cci_slot[0].cc_status =
				((buf[1] & 0x1) == 0) ?
					CHIPCARD_NOT_PRESENT :
					CHIPCARD_PRESENT;
		}
		break;
	case RDR_to_PC_HardwareError:
		trace(ccid, "     : HALT AND CATCH FIRE!!\n");
		break;
	default:
		fprintf(stderr, "*** error: unknown interrupt packet\n");
		break;
	}

	return 1;
}

unsigned int _RDR_to_PC_DataBlock(struct _ccid *ccid, struct _xfr *xfr)
{
	assert(xfr->x_rxhdr->bMessageType == RDR_to_PC_DataBlock);
	trace(ccid, "     : RDR_to_PC_DataBlock: %zu bytes\n", xfr->x_rxlen);
	_hex_dumpf(ccid->cci_tf, xfr->x_rxbuf, xfr->x_rxlen, 16);
	/* APDU chaining parameter */
	return xfr->x_rxhdr->in.bApp;
}

unsigned int _RDR_to_PC_SlotStatus(struct _ccid *ccid, struct _xfr *xfr)
{
	assert(xfr->x_rxhdr->bMessageType == RDR_to_PC_SlotStatus);

	trace(ccid, "     : RDR_to_PC_SlotStatus: ");
	switch( xfr->x_rxhdr->in.bApp ) {
	case 0x00:
		trace(ccid, "Clock running\n");
		return CHIPCARD_CLOCK_START;
	case 0x01:
		trace(ccid, "Clock stopped in L state\n");
		return CHIPCARD_CLOCK_STOP_L;
	case 0x02:
		trace(ccid, "Clock stopped in H state\n");
		return CHIPCARD_CLOCK_STOP_H;
	case 0x03:
		trace(ccid, "Clock stopped in unknown state\n");
		return CHIPCARD_CLOCK_STOP;
	default:
		return CHIPCARD_CLOCK_ERR;
	}
}

static const struct {
	uint16_t fi; /* Fi */
	uint16_t fmax; /* Fmax (KHz) */
} fi_table[16] = {
	{.fi = 372, .fmax = 4000},
	{.fi = 372, .fmax = 5000},
	{.fi = 558, .fmax = 6000},
	{.fi = 744, .fmax = 8000},
	{.fi = 1116, .fmax = 12000},
	{.fi = 1488, .fmax = 16000},
	{.fi = 1860, .fmax = 20000},
	{.fi = 0, },

	{.fi = 0, },
	{.fi = 512, .fmax = 5000},
	{.fi = 768, .fmax = 7500},
	{.fi = 1024, .fmax = 10000},
	{.fi = 1536, .fmax = 15000},
	{.fi = 2048, .fmax = 20000},
	{.fi = 0, },
	{.fi = 0, },
};

static const uint8_t di_table[16] = {
	0, 1, 2, 4, 8, 16, 32, 64,
	12, 20, 0, 0, 0, 0, 0, 0
};

static unsigned int guard_time(uint8_t t)
{
	if ( t == 0xff )
		t = 0;
	return 12 + t;
}

int _RDR_to_PC_Parameters(struct _ccid *ccid, struct _xfr *xfr)
{
	const struct ccid_t0 *t0;
	const struct ccid_t1 *t1;

	assert(xfr->x_rxhdr->bMessageType == RDR_to_PC_Parameters);
	trace(ccid, "     : RDR_to_PC_Parameters: \n");
	switch(xfr->x_rxhdr->in.bApp) {
	case CCID_PROTOCOL_T0:
		trace(ccid, "     : T=0 Protocol block follows\n");
		if ( xfr->x_rxlen < sizeof(*t0) )
			return 0;
		t0 = (const struct ccid_t0 *)xfr->x_rxbuf;

		trace(ccid, "     : Fi=%d Fmax = %dKHz\n",
			fi_table[t0->bmFindexDindex >> 4].fi,
			fi_table[t0->bmFindexDindex >> 4].fmax);
		trace(ccid, "     : Baud rate conversion factor Di = %d\n",
			di_table[t0->bmFindexDindex & 0xf]);
		trace(ccid, "     : %s convention\n",
			(t0->bmTCCSKST0 & 1) ? "Inverse" : "Direct");
		trace(ccid, "     : Guard time: %detu\n",
			guard_time(t0->bGuardTimeT0));
		trace(ccid, "     : Waiting integer: %d\n",
			t0->bWaitingIntegerT0);
		trace(ccid, "     : Clock stop: %d\n",
			t0->bClockStop);

		break;
	case CCID_PROTOCOL_T1:
		trace(ccid, "T=1 Protocol block follows\n");
		if ( xfr->x_rxlen < sizeof(*t1) )
			return 0;
		t1 = (const struct ccid_t1 *)xfr->x_rxbuf;
		_hex_dumpf(ccid->cci_tf, (uint8_t *)t1,
				sizeof(*t1), sizeof(*t1));
		break;
	default:
		trace(ccid, "Unknown rotocol: 0x%.2x\n", xfr->x_rxhdr->in.bApp);
		return 0;
	}

	return 1;
}

static int _cmd_result(struct _ccid *ccid, const struct ccid_msg *msg)
{
	switch( msg->in.bStatus & CCID_STATUS_RESULT_MASK ) {
	case CCID_RESULT_SUCCESS:
		trace(ccid, "     : Command: SUCCESS\n");
		return 1;
	case CCID_RESULT_ERROR:
		switch ( msg->in.bError ) {
		case CCID_ERR_ABORT:
			trace(ccid, "     : Command: ERR: ICC Aborted\n");
			break;
		case CCID_ERR_MUTE:
			trace(ccid, "     : Command: ERR: ICC Timed Out\n");
			break;
		case CCID_ERR_PARITY:
			trace(ccid, "     : Command: ERR: ICC Parity Error\n");
			break;
		case CCID_ERR_OVERRUN:
			trace(ccid, "     : Command: ERR: ICC Buffer Overrun\n");
			break;
		case CCID_ERR_HARDWARE:
			trace(ccid, "     : Command: ERR: Hardware Error\n");
			break;
		case CCID_ERR_BAD_TS:
			trace(ccid, "     : Command: ERR: Bad ATR TS\n");
			break;
		case CCID_ERR_BAD_TCK:
			trace(ccid, "     : Command: ERR: Bad ATR TCK\n");
			break;
		case CCID_ERR_PROTOCOL:
			trace(ccid, "     : Command: ERR: "
					"Unsupported Protocol\n");
			break;
		case CCID_ERR_CLASS:
			trace(ccid, "     : Command: ERR: Unsupported CLA\n");
			break;
		case CCID_ERR_PROCEDURE:
			trace(ccid, "     : Command: ERR: "
					"Procedure Byte Conflict\n");
			break;
		case CCID_ERR_DEACTIVATED:
			trace(ccid, "     : Command: ERR: "
					"Deactivated Protocl\n");
			break;
		case CCID_ERR_AUTO_SEQ:
			trace(ccid, "     : Command: ERR: "
					"Busy with Auto-Sequencing\n");
			break;
		case CCID_ERR_PIN_TIMEOUT:
			trace(ccid, "     : Command: ERR: PIN timeout\n");
			break;
		case CCID_ERR_BUSY:
			trace(ccid, "     : Command: ERR: Slot Busy\n");
			break;
		case CCID_ERR_USR_MIN ... CCID_ERR_USR_MAX:
			trace(ccid, "     : Command: FAILED (0x%.2x)\n",
				msg->in.bError);
			break;
		default:
			trace(ccid, "     : Command: FAILED (RESERVED 0x%.2x)\n",
				msg->in.bError);
			break;
		}
		return 0;
	case CCID_RESULT_TIMEOUT:
		trace(ccid, "     : Command: Time Extension Request\n");
		trace(ccid, "     : BW1/CW1 = 0x%.2x\n", msg->in.bError);
		return 0;
	default:
		fprintf(stderr, "*** error: unknown command result\n");
		return 0;
	}
}

static int do_recv(struct _ccid *ccid, struct _xfr *xfr)
{
	int ret;
	size_t len;

	if ( libusb_bulk_transfer(ccid->cci_dev, ccid->cci_inp,
				(void *)xfr->x_rxhdr,
				x_rbuflen(xfr), &ret, 0) ) {
		fprintf(stderr, "*** error: libusb_bulk_read()\n");
		return 0;
	}

	len = (size_t)ret;

	if ( len < sizeof(*xfr->x_rxhdr) ) {
		fprintf(stderr, "*** error: truncated CCI msg\n");
		return 0;
	}

	if ( sizeof(*xfr->x_rxhdr) + le32toh(xfr->x_rxhdr->dwLength) > len ) {
		fprintf(stderr, "*** error: bad dwLength in CCI msg\n");
		return 0;
	}

	xfr->x_rxlen = le32toh(xfr->x_rxhdr->dwLength);

	return 1;
}

static void _chipcard_set_status(struct _cci *cc, unsigned int status)
{
	switch( status & CCID_SLOT_STATUS_MASK ) {
	case CCID_STATUS_ICC_ACTIVE:
		trace(cc->cc_parent, "     : ICC present and active\n");
		cc->cc_status = CHIPCARD_ACTIVE;
		break;
	case CCID_STATUS_ICC_PRESENT:
		trace(cc->cc_parent, "     : ICC present and inactive\n");
		cc->cc_status = CHIPCARD_PRESENT;
		break;
	case CCID_STATUS_ICC_NOT_PRESENT:
		trace(cc->cc_parent, "     : ICC not presnt\n");
		cc->cc_status = CHIPCARD_NOT_PRESENT;
		break;
	default:
		fprintf(stderr, "*** error: unknown chipcard status update\n");
		break;
	}
}

int _RDR_to_PC(struct _ccid *ccid, unsigned int slot, struct _xfr *xfr)
{
	const struct ccid_msg *msg;
	unsigned int try = 10;

again:
	if ( !do_recv(ccid, xfr) )
		return 0;

	msg = xfr->x_rxhdr;

	trace(ccid, " Recv: %zu bytes for slot %u (seq = 0x%.2x)\n",
		xfr->x_rxlen, msg->bSlot, msg->bSeq);

	if ( msg->bSlot != slot ) {
		fprintf(stderr, "*** error: bad slot %u (expected %u)\n",
			msg->bSlot, slot);
		return 0;
	}

	if ( (uint8_t)(msg->bSeq + 1) != ccid->cci_seq ) {
		fprintf(stderr, "*** error: expected seq 0x%.2x got 0x%.2x\n",
			ccid->cci_seq, (uint8_t)(msg->bSeq + 1));
		return 0;
	}

	_chipcard_set_status(&ccid->cci_slot[msg->bSlot], msg->in.bStatus);

	if ( xfr->x_rxhdr->in.bStatus == CCID_RESULT_TIMEOUT && --try )
		goto again;

	return _cmd_result(ccid, xfr->x_rxhdr);
}

static int _PC_to_RDR(struct _ccid *ccid, unsigned int slot, struct _xfr *xfr)
{
	int ret;
	size_t len;

	/* Escape functions may use bad slots as part of their
	 * interface. For example this is useful in detecting the
	 * presence or absense of specific vendor extensions
	 */
	if ( xfr->x_txhdr->bMessageType != PC_to_RDR_Escape ) {
		assert(slot < ccid->cci_num_slots);
	}

	xfr->x_txhdr->dwLength = le32toh(xfr->x_txlen);
	xfr->x_txhdr->bSlot = slot;
	xfr->x_txhdr->bSeq = ccid->cci_seq++;

	if ( libusb_bulk_transfer(ccid->cci_dev, ccid->cci_outp,
				(void *)xfr->x_txhdr,
				x_tbuflen(xfr), &ret, 0) ) {
		fprintf(stderr, "*** error: libusb_bulk_write()\n");
		return 0;
	}

	len = (size_t)ret;

	if ( ret < x_tbuflen(xfr) ) {
		fprintf(stderr, "*** error: truncated TX: %zu/%zu\n",
			len, x_tbuflen(xfr));
		return 0;
	}

	return 1;
}

int _PC_to_RDR_XfrBlock(struct _ccid *ccid, unsigned int slot, struct _xfr *xfr)
{
	int ret;

	memset(xfr->x_txhdr, 0, sizeof(*xfr->x_txhdr));
	xfr->x_txhdr->bMessageType = PC_to_RDR_XfrBlock;
	//xfr->x_txhdr->out.bApp[0] = 0xff; /* wait integer */
	ret = _PC_to_RDR(ccid, slot, xfr);
	if ( ret ) {
		trace(ccid, " Xmit: PC_to_RDR_XfrBlock(%u)\n", slot);
		_hex_dumpf(ccid->cci_tf, xfr->x_txbuf, xfr->x_txlen, 16);
	}
	return ret;
}

int _PC_to_RDR_Escape(struct _ccid *ccid, unsigned int slot, struct _xfr *xfr)
{
	int ret;

	memset(xfr->x_txhdr, 0, sizeof(*xfr->x_txhdr));
	xfr->x_txhdr->bMessageType = PC_to_RDR_Escape;
	ret = _PC_to_RDR(ccid, slot, xfr);
	if ( ret ) {
		trace(ccid, " Xmit: PC_to_RDR_Escape(%u)\n", slot);
		_hex_dumpf(ccid->cci_tf, xfr->x_txbuf, xfr->x_txlen, 16);
	}
	return ret;
}

int _PC_to_RDR_GetSlotStatus(struct _ccid *ccid, unsigned int slot,
				struct _xfr *xfr)
{
	int ret;

	memset(xfr->x_txhdr, 0, sizeof(*xfr->x_txhdr));
	xfr->x_txhdr->bMessageType = PC_to_RDR_GetSlotStatus;
	ret = _PC_to_RDR(ccid, slot, xfr);
	if ( ret )
		trace(ccid, " Xmit: PC_to_RDR_GetSlotStatus(%u)\n", slot);

	return ret;
}

int _PC_to_RDR_GetParameters(struct _ccid *ccid, unsigned int slot,
				struct _xfr *xfr)
{
	int ret;

	memset(xfr->x_txhdr, 0, sizeof(*xfr->x_txhdr));
	xfr->x_txhdr->bMessageType = PC_to_RDR_GetParameters;
	ret = _PC_to_RDR(ccid, slot, xfr);
	if ( ret )
		trace(ccid, " Xmit: PC_to_RDR_GetParameters(%u)\n", slot);

	return ret;
}

int _PC_to_RDR_SetParameters(struct _ccid *ccid, unsigned int slot,
				struct _xfr *xfr)
{
	int ret;

	memset(xfr->x_txhdr, 0, sizeof(*xfr->x_txhdr));
	xfr->x_txhdr->bMessageType = PC_to_RDR_SetParameters;
	ret = _PC_to_RDR(ccid, slot, xfr);
	if ( ret )
		trace(ccid, " Xmit: PC_to_RDR_SetParameters(%u)\n", slot);

	return ret;
}

int _PC_to_RDR_ResetParameters(struct _ccid *ccid, unsigned int slot,
				struct _xfr *xfr)
{
	int ret;

	memset(xfr->x_txhdr, 0, sizeof(*xfr->x_txhdr));
	xfr->x_txhdr->bMessageType = PC_to_RDR_ResetParameters;
	ret = _PC_to_RDR(ccid, slot, xfr);
	if ( ret )
		trace(ccid, " Xmit: PC_to_RDR_ResetParameters(%u)\n", slot);

	return ret;
}

int _PC_to_RDR_IccPowerOn(struct _ccid *ccid, unsigned int slot,
				struct _xfr *xfr,
				unsigned int voltage)
{
	int ret;

	assert(voltage <= CHIPCARD_1_8V);

	memset(xfr->x_txhdr, 0, sizeof(*xfr->x_txhdr));
	xfr->x_txhdr->bMessageType = PC_to_RDR_IccPowerOn;
	xfr->x_txhdr->out.bApp[0] = voltage & 0xff;
	ret = _PC_to_RDR(ccid, slot, xfr);
	if ( ret ) {
		trace(ccid, " Xmit: PC_to_RDR_IccPowerOn(%u)\n", slot);
		switch(voltage) {
		case CHIPCARD_AUTO_VOLTAGE:
			trace(ccid, "     : Automatic Voltage Selection\n");
			break;
		case CHIPCARD_5V:
			trace(ccid, "     : 5 Volts\n");
			break;
		case CHIPCARD_3V:
			trace(ccid, "     : 3 Volts\n");
			break;
		case CHIPCARD_1_8V:
			trace(ccid, "     : 1.8 Volts\n");
			break;
		}
	}

	return ret;
}

int _PC_to_RDR_IccPowerOff(struct _ccid *ccid, unsigned int slot,
				struct _xfr *xfr)
{
	int ret;

	memset(xfr->x_txhdr, 0, sizeof(*xfr->x_txhdr));
	xfr->x_txhdr->bMessageType = PC_to_RDR_IccPowerOff;
	ret = _PC_to_RDR(ccid, slot, xfr);
	if ( ret ) {
		trace(ccid, " Xmit: PC_to_RDR_IccPowerOff(%u)\n", slot);
	}

	return ret;
}

static void byteswap_desc(struct ccid_desc *desc)
{
#define _SWAP(field, func) desc->field = func (desc->field)
	_SWAP(bcdCCID, le16toh);
	_SWAP(dwProtocols, le32toh);
	_SWAP(dwDefaultClock, le32toh);
	_SWAP(dwMaximumClock, le32toh);
	_SWAP(dwDataRate, le32toh);
	_SWAP(dwMaxDataRate, le32toh);
	_SWAP(dwMaxIFSD, le32toh);
	_SWAP(dwSynchProtocols, le32toh);
	_SWAP(dwMechanical, le32toh);
	_SWAP(dwFeatures, le32toh);
	_SWAP(dwMaxCCIDMessageLength, le32toh);
	_SWAP(wLcdLayout, le16toh);
#undef _SWAP
}

static unsigned int bcd_hi(uint16_t word)
{
	return (((word & 0xf000) >> 12) * 10) + ((word & 0xf00) >> 8);
}

static unsigned int bcd_lo(uint16_t word)
{
	return (((word & 0xf0) >> 4) * 10) + (word & 0xf);
}


static int get_endpoint(struct _ccid *ccid, const uint8_t *ptr, size_t len)
{
	const struct libusb_endpoint_descriptor *ep;

	if ( len < LIBUSB_DT_ENDPOINT_SIZE )
		return 0;

	ep = (const struct libusb_endpoint_descriptor *)ptr;

	switch( ep->bmAttributes ) {
	case LIBUSB_TRANSFER_TYPE_BULK:
		trace(ccid, " o Bulk %s endpoint at 0x%.2x max=%u bytes\n",
			(ep->bEndpointAddress &  LIBUSB_ENDPOINT_DIR_MASK) ?
				"IN" : "OUT",
			ep->bEndpointAddress & LIBUSB_ENDPOINT_ADDRESS_MASK,
			le16toh(ep->wMaxPacketSize));
		if ( ep->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) {
			ccid->cci_inp = ep->bEndpointAddress;
			ccid->cci_max_in = le16toh(ep->wMaxPacketSize);
		}else{
			ccid->cci_outp = ep->bEndpointAddress;
			ccid->cci_max_out = le16toh(ep->wMaxPacketSize);
		}
		break;
	case LIBUSB_TRANSFER_TYPE_INTERRUPT:
		trace(ccid, " o Interrupt %s endpint 0x%.2x nax=%u bytes\n",
			(ep->bEndpointAddress &  LIBUSB_ENDPOINT_DIR_MASK) ?
				"IN" : "OUT",
			ep->bEndpointAddress & LIBUSB_ENDPOINT_ADDRESS_MASK,
			le16toh(ep->wMaxPacketSize));
		ccid->cci_intrp = ep->bEndpointAddress;
		ccid->cci_max_intr = le16toh(ep->wMaxPacketSize);
		break;
	default:
		return 0;
	}

	return 1;
}

static int fill_ccid_desc(struct _ccid *ccid, const uint8_t *ptr, size_t len)
{
	if ( len < sizeof(ccid->cci_desc) ) {
		fprintf(stderr, "*** error: truncated CCID descriptor\n");
		return 0;
	}

	memcpy(&ccid->cci_desc, ptr, sizeof(ccid->cci_desc));

	byteswap_desc(&ccid->cci_desc);

	ccid->cci_num_slots = ccid->cci_desc.bMaxSlotIndex + 1;
	ccid->cci_max_slots = ccid->cci_desc.bMaxCCIDBusySlots;

	trace(ccid, " o got %zu/%zu byte desc of type 0x%.2x\n",
		len, sizeof(ccid->cci_desc), ccid->cci_desc.bDescriptorType);
	trace(ccid, " o CCID v%u.%u device with %u/%u slots in parallel\n",
			bcd_hi(ccid->cci_desc.bcdCCID),
			bcd_lo(ccid->cci_desc.bcdCCID),
			ccid->cci_max_slots, ccid->cci_num_slots);


	trace(ccid, " o Voltages:");
	if ( ccid->cci_desc.bVoltageSupport & CCID_5V )
		trace(ccid, " 5V");
	if ( ccid->cci_desc.bVoltageSupport & CCID_3V )
		trace(ccid, " 3V");
	if ( ccid->cci_desc.bVoltageSupport & CCID_1_8V )
		trace(ccid, " 1.8V");
	trace(ccid, "\n");

	trace(ccid, " o Protocols: ");
	if ( ccid->cci_desc.dwProtocols & CCID_T0 )
		trace(ccid, " T=0");
	if ( ccid->cci_desc.dwProtocols & CCID_T1 )
		trace(ccid, " T=1");
	trace(ccid, "\n");

	trace(ccid, " o Default Clock Freq.: %uKHz\n",
			ccid->cci_desc.dwDefaultClock);
	trace(ccid, " o Max. Clock Freq.: %uKHz\n",
			ccid->cci_desc.dwMaximumClock);
	if ( ccid->cci_desc.bNumClockSupported ) {
		trace(ccid, "   .bNumClocks = %u\n",
				ccid->cci_desc.bNumClockSupported);
	}
	trace(ccid, " o Default Data Rate: %ubps\n",	
			ccid->cci_desc.dwDataRate);
	trace(ccid, " o Max. Data Rate: %ubps\n",
			ccid->cci_desc.dwMaxDataRate);
	if ( ccid->cci_desc.bNumDataRatesSupported ) {
		trace(ccid, "   .bNumDataRates = %u\n",
				ccid->cci_desc.bNumDataRatesSupported);
	}
	trace(ccid, " o T=1 Max. IFSD = %u\n",
			ccid->cci_desc.dwMaxIFSD);
	trace(ccid, "   .dwSynchProtocols = 0x%.8x\n",
			ccid->cci_desc.dwSynchProtocols);
	trace(ccid, "   .dwMechanical = 0x%.8x\n",
			ccid->cci_desc.dwMechanical);
	
	if ( ccid->cci_desc.dwFeatures & CCID_ATR_CONFIG )
		trace(ccid, " o Paramaters configured from ATR\n");
	if ( ccid->cci_desc.dwFeatures & CCID_ACTIVATE )
		trace(ccid, " o Chipcard auto-activate on insert\n");
	if ( ccid->cci_desc.dwFeatures & CCID_VOLTAGE )
		trace(ccid, " o Auto Voltage Select\n");
	if ( ccid->cci_desc.dwFeatures & CCID_FREQ )
		trace(ccid, " o Auto Clock Freq. Select\n");
	if ( ccid->cci_desc.dwFeatures & CCID_BAUD )
		trace(ccid, " o Auto Data Rate Select\n");
	if ( ccid->cci_desc.dwFeatures & CCID_CLOCK_STOP )
		trace(ccid, " o Clock Stop Mode\n");
	if ( ccid->cci_desc.dwFeatures & CCID_NAD )
		trace(ccid, " o NAD value other than 0 permitted\n");
	if ( ccid->cci_desc.dwFeatures & CCID_IFSD )
		trace(ccid, " o Auto IFSD on first exchange\n");
	
	switch ( ccid->cci_desc.dwFeatures & (CCID_PPS_AUTO|CCID_PPS_CUR) ) {
	case CCID_PPS_AUTO:
		trace(ccid, " o Parameter selection: Automatic\n");
		break;
	case CCID_PPS_CUR:
		trace(ccid, " o Parameter selection: Manual\n");
		break;
	default:
		fprintf(stderr, "Manual/Auto param. conflict in descriptor\n");
		return 0;
	}

	switch ( ccid->cci_desc.dwFeatures &
		(CCID_T1_TPDU|CCID_T1_APDU|CCID_T1_APDU_EXT) ) {
	case CCID_T1_TPDU:
		trace(ccid, " o Level: T=1 TPDU\n");
		break;
	case CCID_T1_APDU:
		trace(ccid, " o Level: T=1 APDU (Short)\n");
		break;
	case CCID_T1_APDU_EXT:
		trace(ccid, " o Level: T=1 APDU (Short and Extended)\n");
		break;
	default:
		fprintf(stderr, "T=1 PDU conflict in descriptor\n");
		return 0;
	}


	trace(ccid, " o Max. Message Length: %u bytes\n",
		ccid->cci_desc.dwMaxCCIDMessageLength);

	if ( ccid->cci_desc.dwFeatures & (CCID_T1_APDU|CCID_T1_APDU_EXT) ) {
		trace(ccid, " o APDU Default GetResponse: %u\n",
			ccid->cci_desc.bClassGetResponse);
		trace(ccid, " o APDU Default Envelope: %u\n",
			ccid->cci_desc.bClassEnvelope);
	}

	if ( ccid->cci_desc.wLcdLayout ) {
		trace(ccid, " o LCD Layout: %ux%u\n",
			bcd_hi(ccid->cci_desc.wLcdLayout),
			bcd_lo(ccid->cci_desc.wLcdLayout));
	}

	switch ( ccid->cci_desc.bPINSupport & (CCID_PIN_VER|CCID_PIN_MOD) ) {
	case CCID_PIN_VER:
		trace(ccid, " o PIN verification supported\n");
		break;
	case CCID_PIN_MOD:
		trace(ccid, " o PIN modification supported\n");
		break;
	case CCID_PIN_VER|CCID_PIN_MOD:
		trace(ccid, " o PIN verification and modification supported\n");
		break;
	}

	return 1;
}

static int probe_descriptors(struct _ccid *ccid)
{
	uint8_t dbuf[512];
	uint8_t *ptr, *end;
	int sz, valid_ccid = 0;

	sz = libusb_get_descriptor(ccid->cci_dev, LIBUSB_DT_CONFIG, 0,
				dbuf, sizeof(dbuf));
	if ( sz < 0 )
		return 0;

	trace(ccid, " o Fetching config descriptor\n");

	for(ptr = dbuf, end = ptr + sz; ptr + 2 < end; ) {
		if ( ptr + ptr[0] > end )
			return 0;

		switch( ptr[1] ) {
		case LIBUSB_DT_DEVICE:
			break;
		case LIBUSB_DT_CONFIG:
			break;
		case LIBUSB_DT_STRING:
			break;
		case LIBUSB_DT_INTERFACE:
			break;
		case LIBUSB_DT_ENDPOINT:
			get_endpoint(ccid, ptr, ptr[0]);
			break;
		case CCID_DT:
			if ( fill_ccid_desc(ccid, ptr, ptr[0]) )
				valid_ccid = 1;
			break;
		default:
			trace(ccid, " o Unknown descriptor 0x%.2x\n", ptr[1]);
			break;
		}

		ptr += ptr[0];
		
	}

	if ( !valid_ccid ) {
		fprintf(stderr, "*** error: No CCID descriptor found\n");
		return 0;
	}

	return 1;
}

static int get_data_rates(struct _ccid *ccid)
{
	uint32_t buf[ccid->cci_desc.bNumDataRatesSupported];
	uint8_t rt;
	int ret, i;

	/* no data rates to get... */
	if ( sizeof(buf) == 0 )
		return 1;

	rt = (LIBUSB_ENDPOINT_IN|
		LIBUSB_REQUEST_TYPE_CLASS|LIBUSB_RECIPIENT_INTERFACE);

	ret = libusb_control_transfer(ccid->cci_dev, rt,
					CCID_CTL_GET_DATA_RATES,
					0, ccid->cci_intf,
					(uint8_t *)buf, sizeof(buf),
					1000);
	if ( ret < 0 )	
		return 0;

	ccid->cci_num_rate = ret / 4;

	ccid->cci_data_rate = calloc(ccid->cci_num_rate,
					sizeof(*ccid->cci_data_rate));
	if ( NULL == ccid->cci_data_rate )
		return 0;

	for(i = 0; i < ccid->cci_num_rate; i++) {
		ccid->cci_data_rate[i] = le32toh(buf[i]);
//		trace(ccid, "%"PRId32"bps\n", ccid->cci_data_rate[i]);
	}

	return 1;
}

static int get_clock_freqs(struct _ccid *ccid)
{
	uint32_t buf[ccid->cci_desc.bNumClockSupported];
	uint8_t rt;
	int ret, i;

	/* no data rates to get... */
	if ( sizeof(buf) == 0 )
		return 1;

	rt = (LIBUSB_ENDPOINT_IN|
		LIBUSB_REQUEST_TYPE_CLASS|LIBUSB_RECIPIENT_INTERFACE);

	ret = libusb_control_transfer(ccid->cci_dev, rt,
					CCID_CTL_GET_CLOCK_FREQS,
					0, ccid->cci_intf,
					(uint8_t *)buf, sizeof(buf),
					1000);
	if ( ret < 0 )	
		return 0;

	ccid->cci_num_clock = ret / 4;

	ccid->cci_clock_freq = calloc(ccid->cci_num_clock,
					sizeof(*ccid->cci_clock_freq));
	if ( NULL == ccid->cci_clock_freq )
		return 0;

	trace(ccid, "%zu clock rates supported\n", ccid->cci_num_clock);
	for(i = 0; i < ccid->cci_num_clock; i++) {
		ccid->cci_clock_freq[i] = le32toh(buf[i]);
		trace(ccid, " o %"PRId32"KHz\n", ccid->cci_clock_freq[i]);
	}

	return 1;
}

/** Connect to a physical chipcard device.
 * \ingroup g_ccid
 * @param dev \ref ccidev_t representing a physical device.
 * @param tracefile filename to open for trace logging (or NULL).
 *
 * This function:
 * #- Probes the USB device searching for a valid CCID interface.
 * #- Optionally opens a file for trace logging.
 * #- Perform any device specific initialisation.
 *
 * @return NULL on failure, valid \ref ccid_t object otherwise.
 */
ccid_t ccid_probe(ccidev_t dev, const char *tracefile)
{
	struct _cci_interface intf;
	struct _ccid *ccid = NULL;
	unsigned int x;
	int c;

	if ( !_probe_descriptors(dev, &intf) ) {
		goto out;
	}

	/* First initialize data structures */
	ccid = calloc(1, sizeof(*ccid));
	if ( NULL == ccid )
		goto out;

	if ( tracefile ) {
		if ( !strcmp("-", tracefile) )
			ccid->cci_tf = stdout;
		else
			ccid->cci_tf = fopen(tracefile, "w");
		if ( ccid->cci_tf == NULL )
			goto out_free;
	}

	trace(ccid, "Probe CCI on dev %u:%u %d/%d/%d\n",
		libusb_get_bus_number(dev),
		libusb_get_device_address(dev), intf.c, intf.i, intf.a);
	if ( intf.name )
		trace(ccid, "Recognised as: %s\n", intf.name);

	for(x = 0; x < CCID_MAX_SLOTS; x++) {
		ccid->cci_slot[x].cc_parent = ccid;
		ccid->cci_slot[x].cc_idx = x;
		ccid->cci_slot[x].cc_ops = &_contact_ops;
	}

	for(x = 0; x < RFID_MAX_FIELDS; x++) {
		ccid->cci_rf[x].cc_parent = ccid;
		ccid->cci_rf[x].cc_ops = &_rfid_ops;
		/* idx and ops set by proprietary initialisation routines */
	}

	/* Second, open USB device and get it ready */
	if ( libusb_open(dev, &ccid->cci_dev) ) {
		goto out_free;
	}

#if 1
	libusb_reset_device(ccid->cci_dev);
#endif

	if ( libusb_get_configuration(ccid->cci_dev, &c) ) {
		trace(ccid, "error getting configuration\n");
		goto out_close;
	}

	if ( intf.c != c && libusb_set_configuration(ccid->cci_dev, intf.c) ) {
		trace(ccid, "error setting configuration\n");
		goto out_close;
	}

	if ( libusb_get_configuration(ccid->cci_dev, &c) ) {
		trace(ccid, "error getting configuration\n");
		goto out_close;
	}

	if ( c != intf.c ) {
		trace(ccid, "raced while setting configuration (%d != %d)\n",
			c, intf.c);
		goto out_close;
	}

	if ( libusb_claim_interface(ccid->cci_dev, intf.i) ) {
		trace(ccid, "error claiming interface\n");
		goto out_close;
	}

	if ( libusb_set_interface_alt_setting(ccid->cci_dev, intf.i, intf.a) ) {
		trace(ccid, "error setting alternate settings\n");
		goto out_close;
	}

	ccid->cci_intf = intf.i;

	/* Third, probe the CCID class descriptor */
	if( !probe_descriptors(ccid) ) {
		goto out_close;
	}

	if ( !get_data_rates(ccid) )
		goto out_close;
	if( !get_clock_freqs(ccid) )
		goto out_close;

	ccid->cci_xfr = _xfr_do_alloc(ccid->cci_max_out, ccid->cci_max_in);
	if ( NULL == ccid->cci_xfr )
		goto out_close;

	/* Fourth, setup each slot */
	trace(ccid, "Setting up %u contact card slots\n", ccid->cci_num_slots);
	for(x = 0; x < ccid->cci_num_slots; x++) {
		if ( !_PC_to_RDR_GetSlotStatus(ccid, x, ccid->cci_xfr) )
			goto out_freebuf;
		if ( !_RDR_to_PC(ccid, x, ccid->cci_xfr) )
			goto out_freebuf;
		if ( !_RDR_to_PC_SlotStatus(ccid, ccid->cci_xfr) )
			goto out_freebuf;
	}

	/* Fifth, Initialise any proprietary interfaces */
//	if ( intf.flags & INTF_RFID_OMNI )
//		_omnikey_init_prox(ccid);

	ccid->cci_bus = libusb_get_bus_number(dev);
	ccid->cci_addr = libusb_get_device_address(dev);
	ccid->cci_name = strdup(intf.name);
	goto out;

out_freebuf:
	_xfr_do_free(ccid->cci_xfr);
out_close:
	libusb_close(ccid->cci_dev);
out_free:
	free(ccid);
	ccid = NULL;
	fprintf(stderr, "ccid: error probing device\n");
out:
	return ccid;
}

uint8_t ccid_bus(ccid_t ccid)
{
	return ccid->cci_bus;
}

uint8_t ccid_addr(ccid_t ccid)
{
	return ccid->cci_addr;
}

const char *ccid_name(ccid_t ccid)
{
	return ccid->cci_name;
}

/** Close connection to a chip card device.
 * \ingroup g_ccid
 * @param ccid The \ref ccid_t to close.
 * Closes connection to physical defice and frees the \ref ccid_t. Note that
 * any transaction buffers will have to be destroyed seperately. All references
 * to slots will become invalid.
 */
void ccid_close(ccid_t ccid)
{
	unsigned int i;

	if ( ccid ) {
		if ( ccid->cci_dev )
			libusb_close(ccid->cci_dev);
		if ( ccid->cci_tf )
			fclose(ccid->cci_tf);
		_xfr_do_free(ccid->cci_xfr);
		free(ccid->cci_name);

		for(i = 0; i < ccid->cci_num_rf; i++) {
			if ( NULL == ccid->cci_rf[i].cc_ops->dtor )
				continue;
			(*ccid->cci_rf[i].cc_ops->dtor)(ccid->cci_rf + i);
		}
	}
	free(ccid);
}

/** Retrieve the number of slots in the CCID.
 * \ingroup g_ccid
 * @param ccid The \ref ccid_t to return number of slots for.
 * @return The number of slots.
 */
unsigned int ccid_num_slots(ccid_t ccid)
{
	return ccid->cci_num_slots;
}

/** Retrieve a handle to a CCID slot.
 * \ingroup g_ccid
 * @param ccid The \ref ccid_t containing the required slot.
 * @param num The slot number to retrieve (zero-based).
 * @return \ref cci_t rerpesenting the required slot.
 */
cci_t ccid_get_slot(ccid_t ccid, unsigned int num)
{
	if ( num < ccid->cci_num_slots ) {
		return ccid->cci_slot + num;
	}else{
		return NULL;
	}
}

/** Retrieve the number of RF fields in the CCID.
 * \ingroup g_ccid
 * @param ccid The \ref ccid_t to return number of fields for.
 * @return The number of fields.
 */
unsigned int ccid_num_fields(ccid_t ccid)
{
	return ccid->cci_num_rf;
}

/** Retrieve a handle to a RFID field.
 * \ingroup g_ccid
 * @param ccid The \ref ccid_t containing the required field.
 * @param num The field number to retrieve (zero-based).
 * @return \ref cci_t rerpesenting the required field.
 */
cci_t ccid_get_field(ccid_t ccid, unsigned int num)
{
	if ( num < ccid->cci_num_rf ) {
		return ccid->cci_rf + num;
	}else{
		return NULL;
	}
}

/** Print a message in the trace log
 * \ingroup g_ccid
 * @param ccid The \ref ccid_t to which the log message is pertinent.
 * @param fmt Format string as per printf().
 *
 * Prints a message in the trace log of the specified CCI device. Does nothing
 * if NULL was passed as tracefile paramater of \ref cci_open.
 */
void ccid_log(ccid_t ccid, const char *fmt, ...)
{
	va_list va;

	if ( NULL == ccid->cci_tf )
		return;

	va_start(va, fmt);
	vfprintf(ccid->cci_tf, fmt, va);
	va_end(va);
}
