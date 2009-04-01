/*
 * This file is part of cci-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
 *
 * Chip Card Interface Device API. Contains USB CCID driver.
*/

#include <ccid.h>

#include <stdarg.h>

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
	const uint8_t *buf;
	int ret;
	size_t len;

	ret = usb_interrupt_read(cci->cci_dev, cci->cci_intrp,
				(void *)cci->cci_xfr->x_rxhdr,
				x_rbuflen(cci->cci_xfr), 250);
	if ( ret < 0 )
		return 0;
	
	if ( ret == 0 )
		return 1;

	buf = (void *)cci->cci_xfr->x_rxhdr;
	len = (size_t)ret;

	trace(cci, " Intr: %u byte interrupt packet\n", len);
	if ( len < 1 )
		return 1;

	switch( buf[0] ) {
	case RDR_to_PC_NotifySlotChange:
		assert(2 == len);
		if ( buf[1] & 0x2 ) {
			trace(cci, "     : Slot status changed to %s\n",
				((buf[1] & 0x1) == 0) ? 
					"NOT present" : "present");
			cci->cci_slot[0].cc_status =
				((buf[1] & 0x1) == 0) ?
					CHIPCARD_NOT_PRESENT :
					CHIPCARD_PRESENT;
		}
		break;
	case RDR_to_PC_HardwareError:
		trace(cci, "     : HALT AND CATCH FIRE!!\n");
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
	trace(cci, "     : RDR_to_PC_DataBlock: %u bytes\n", xfr->x_rxlen);
	_hex_dumpf(cci->cci_tf, xfr->x_rxbuf, xfr->x_rxlen, 16);
	/* APDU chaining parameter */
	return xfr->x_rxhdr->in.bApp;
}

unsigned int _RDR_to_PC_SlotStatus(struct _cci *cci, struct _xfr *xfr)
{
	assert(xfr->x_rxhdr->bMessageType == RDR_to_PC_SlotStatus);

	trace(cci, "     : RDR_to_PC_SlotStatus: ");
	switch( xfr->x_rxhdr->in.bApp ) {
	case 0x00:
		trace(cci, "Clock running\n");
		return CHIPCARD_CLOCK_START;
	case 0x01:
		trace(cci, "Clock stopped in L state\n");
		return CHIPCARD_CLOCK_STOP_L;
	case 0x02:
		trace(cci, "Clock stopped in H state\n");
		return CHIPCARD_CLOCK_STOP_H;
	case 0x03:
		trace(cci, "Clock stopped in unknown state\n");
		return CHIPCARD_CLOCK_STOP;
	default:
		return CHIPCARD_CLOCK_ERR;
	}
}

static int _cmd_result(struct _cci *cci, const struct ccid_msg *msg)
{
	switch( msg->in.bStatus & CCID_STATUS_RESULT_MASK ) {
	case CCID_RESULT_SUCCESS:
		trace(cci, "     : Command: SUCCESS\n");
		return 1;
	case CCID_RESULT_ERROR:
		switch ( msg->in.bError ) {
		case CCID_ERR_ABORT:
			trace(cci, "     : Command: ERR: ICC Aborted\n");
			break;
		case CCID_ERR_MUTE:
			trace(cci, "     : Command: ERR: ICC Timed Out\n");
			break;
		case CCID_ERR_PARITY:
			trace(cci, "     : Command: ERR: ICC Parity Error\n");
			break;
		case CCID_ERR_OVERRUN:
			trace(cci, "     : Command: ERR: ICC Buffer Overrun\n");
			break;
		case CCID_ERR_HARDWARE:
			trace(cci, "     : Command: ERR: Hardware Error\n");
			break;
		case CCID_ERR_BAD_TS:
			trace(cci, "     : Command: ERR: Bad ATR TS\n");
			break;
		case CCID_ERR_BAD_TCK:
			trace(cci, "     : Command: ERR: Bad ATR TCK\n");
			break;
		case CCID_ERR_PROTOCOL:
			trace(cci, "     : Command: ERR: "
					"Unsupported Protocol\n");
			break;
		case CCID_ERR_CLASS:
			trace(cci, "     : Command: ERR: Unsupported CLA\n");
			break;
		case CCID_ERR_PROCEDURE:
			trace(cci, "     : Command: ERR: "
					"Procedure Byte Conflict\n");
			break;
		case CCID_ERR_DEACTIVATED:
			trace(cci, "     : Command: ERR: "
					"Deactivated Protocl\n");
			break;
		case CCID_ERR_AUTO_SEQ:
			trace(cci, "     : Command: ERR: "
					"Busy with Auto-Sequencing\n");
			break;
		case CCID_ERR_PIN_TIMEOUT:
			trace(cci, "     : Command: ERR: PIN timeout\n");
			break;
		case CCID_ERR_BUSY:
			trace(cci, "     : Command: ERR: Slot Busy\n");
			break;
		case CCID_ERR_USR_MIN ... CCID_ERR_USR_MAX:
			trace(cci, "     : Command: FAILED (0x%.2x)\n",
				msg->in.bError);
			break;
		default:
			trace(cci, "     : Command: FAILED (RESERVED 0x%.2x)\n",
				msg->in.bError);
			break;
		}
		return 0;
	case CCID_RESULT_TIMEOUT:
		trace(cci, "     : Command: Time Extension Request\n");
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

static void _chipcard_set_status(struct _chipcard *cc, unsigned int status)
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

int _RDR_to_PC(struct _cci *cci, unsigned int slot, struct _xfr *xfr)
{
	const struct ccid_msg *msg;
	unsigned int try = 10;

again:
	if ( !do_recv(cci, xfr) )
		return 0;

	msg = xfr->x_rxhdr;

	trace(cci, " Recv: %d bytes for slot %u (seq = 0x%.2x)\n",
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

	if ( xfr->x_rxhdr->in.bStatus == CCID_RESULT_TIMEOUT && try )
		goto again;

	return _cmd_result(cci, xfr->x_rxhdr);
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
		trace(cci, " Xmit: PC_to_RDR_XfrBlock(%u)\n", slot);
		_hex_dumpf(cci->cci_tf, xfr->x_txbuf, xfr->x_txlen, 16);
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
		trace(cci, " Xmit: PC_to_RDR_GetSlotStatus(%u)\n", slot);

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
		trace(cci, " Xmit: PC_to_RDR_IccPowerOn(%u)\n", slot);
		switch(voltage) {
		case CHIPCARD_AUTO_VOLTAGE:
			trace(cci, "     : Automatic Voltage Selection\n");
			break;
		case CHIPCARD_5V:
			trace(cci, "     : 5 Volts\n");
			break;
		case CHIPCARD_3V:
			trace(cci, "     : 3 Volts\n");
			break;
		case CHIPCARD_1_8V:
			trace(cci, "     : 1.8 Volts\n");
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
		trace(cci, " Xmit: PC_to_RDR_IccPowerOff(%u)\n", slot);
	}

	return ret;
}

static void byteswap_desc(struct ccid_desc *desc)
{
#define _SWAP(field, func) desc->field = func (desc->field)
	_SWAP(bcdCCID, sys_le16);
	_SWAP(dwProtocols, sys_le32);
	_SWAP(dwDefaultClock, sys_le32);
	_SWAP(dwMaximumClock, sys_le32);
	_SWAP(dwDataRate, sys_le32);
	_SWAP(dwMaxDataRate, sys_le32);
	_SWAP(dwMaxIFSD, sys_le32);
	_SWAP(dwSynchProtocols, sys_le32);
	_SWAP(dwMechanical, sys_le32);
	_SWAP(dwFeatures, sys_le32);
	_SWAP(dwMaxCCIDMessageLength, sys_le32);
	_SWAP(wLcdLayout, sys_le16);
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


static int get_endpoint(struct _cci *cci, const uint8_t *ptr, size_t len)
{
	const struct usb_endpoint_descriptor *ep;

	if ( len < USB_DT_ENDPOINT_SIZE )
		return 0;

	ep = (const struct usb_endpoint_descriptor *)ptr;

	switch( ep->bmAttributes ) {
	case USB_ENDPOINT_TYPE_BULK:
		trace(cci, " o Bulk %s endpoint at 0x%.2x max=%u bytes\n",
			(ep->bEndpointAddress &  USB_ENDPOINT_DIR_MASK) ?
				"IN" : "OUT",
			ep->bEndpointAddress & USB_ENDPOINT_ADDRESS_MASK,
			sys_le16(ep->wMaxPacketSize));
		if ( ep->bEndpointAddress & USB_ENDPOINT_DIR_MASK) {
			cci->cci_inp = ep->bEndpointAddress &
					USB_ENDPOINT_ADDRESS_MASK;
			cci->cci_max_in = sys_le16(ep->wMaxPacketSize);
		}else{
			cci->cci_outp = ep->bEndpointAddress &
					USB_ENDPOINT_ADDRESS_MASK;
			cci->cci_max_out = sys_le16(ep->wMaxPacketSize);
		}
		break;
	case USB_ENDPOINT_TYPE_INTERRUPT:
		trace(cci, " o Interrupt %s endpint 0x%.2x nax=%u bytes\n",
			(ep->bEndpointAddress &  USB_ENDPOINT_DIR_MASK) ?
				"IN" : "OUT",
			ep->bEndpointAddress & USB_ENDPOINT_ADDRESS_MASK,
			sys_le16(ep->wMaxPacketSize));
		cci->cci_intrp = ep->bEndpointAddress &
					USB_ENDPOINT_ADDRESS_MASK;
		cci->cci_max_intr = sys_le16(ep->wMaxPacketSize);
		break;
	default:
		return 0;
	}

	return 1;
}

static int fill_ccid_desc(struct _cci *cci, const uint8_t *ptr, size_t len)
{
	if ( len < sizeof(cci->cci_desc) ) {
		fprintf(stderr, "*** error: truncated CCID descriptor\n");
		return 0;
	}

	/* FIXME: scan for CCID descriptor */
	memcpy(&cci->cci_desc, ptr, sizeof(cci->cci_desc));

	byteswap_desc(&cci->cci_desc);

	cci->cci_num_slots = cci->cci_desc.bMaxSlotIndex + 1;
	cci->cci_max_slots = cci->cci_desc.bMaxCCIDBusySlots;

	trace(cci, " o got %u/%u byte desc of type 0x%.2x\n",
		len, sizeof(cci->cci_desc), cci->cci_desc.bDescriptorType);
	trace(cci, " o CCID version %u.%u device with %u/%u slots in parallel\n",
			bcd_hi(cci->cci_desc.bcdCCID),
			bcd_lo(cci->cci_desc.bcdCCID),
			cci->cci_max_slots, cci->cci_num_slots);


	trace(cci, " o Voltages:");
	if ( cci->cci_desc.bVoltageSupport & CCID_5V )
		trace(cci, " 5V");
	if ( cci->cci_desc.bVoltageSupport & CCID_3V )
		trace(cci, " 3V");
	if ( cci->cci_desc.bVoltageSupport & CCID_1_8V )
		trace(cci, " 1.8V");
	trace(cci, "\n");

	trace(cci, " o Protocols: ");
	if ( cci->cci_desc.dwProtocols & CCID_T0 )
		trace(cci, " T=0");
	if ( cci->cci_desc.dwProtocols & CCID_T1 )
		trace(cci, " T=1");
	trace(cci, "\n");

	trace(cci, " o Default Clock Freq.: %uKHz\n", cci->cci_desc.dwDefaultClock);
	trace(cci, " o Max. Clock Freq.: %uKHz\n", cci->cci_desc.dwMaximumClock);
	//trace(cci, "   .bNumClocks = %u\n", cci->cci_desc.bNumClockSupported);
	trace(cci, " o Default Data Rate: %ubps\n", cci->cci_desc.dwDataRate);
	trace(cci, " o Max. Data Rate: %ubps\n", cci->cci_desc.dwMaxDataRate);
	//trace(cci, "   .bNumDataRates = %u\n", cci->cci_desc.bNumDataRatesSupported);
	trace(cci, " o T=1 Max. IFSD = %u\n", cci->cci_desc.dwMaxIFSD);
	//trace(cci, "   .dwSynchProtocols = 0x%.8x\n",
	//	cci->cci_desc.dwSynchProtocols);
	//trace(cci, "   .dwMechanical = 0x%.8x\n", cci->cci_desc.dwMechanical);
	
	if ( cci->cci_desc.dwFeatures & CCID_ATR_CONFIG )
		trace(cci, " o Paramaters configured from ATR\n");
	if ( cci->cci_desc.dwFeatures & CCID_ACTIVATE )
		trace(cci, " o Chipcard auto-activate\n");
	if ( cci->cci_desc.dwFeatures & CCID_VOLTAGE )
		trace(cci, " o Auto Voltage Select\n");
	if ( cci->cci_desc.dwFeatures & CCID_FREQ )
		trace(cci, " o Auto Clock Freq. Select\n");
	if ( cci->cci_desc.dwFeatures & CCID_BAUD )
		trace(cci, " o Auto Data Rate Select\n");
	if ( cci->cci_desc.dwFeatures & CCID_CLOCK_STOP )
		trace(cci, " o Clock Stop Mode\n");
	if ( cci->cci_desc.dwFeatures & CCID_NAD )
		trace(cci, " o NAD value other than 0 permitted\n");
	if ( cci->cci_desc.dwFeatures & CCID_IFSD )
		trace(cci, " o Auto IFSD on first exchange\n");
	
	switch ( cci->cci_desc.dwFeatures & (CCID_PPS_VENDOR|CCID_PPS) ) {
	case CCID_PPS_VENDOR:
		trace(cci, " o Proprietary parameter selection algorithm\n");
		break;
	case CCID_PPS:
		trace(cci, " o Chipcard automatic PPD\n");
		break;
	default:
		fprintf(stderr, "PPS/PPS_VENDOR conflict in descriptor\n");
		return 0;
	}

	switch ( cci->cci_desc.dwFeatures &
		(CCID_T1_TPDU|CCID_T1_APDU|CCID_T1_APDU_EXT) ) {
	case CCID_T1_TPDU:
		trace(cci, " o T=1 TPDU\n");
		break;
	case CCID_T1_APDU:
		trace(cci, " o T=1 APDU (Short)\n");
		break;
	case CCID_T1_APDU_EXT:
		trace(cci, " o T=1 APDU (Short and Extended)\n");
		break;
	default:
		fprintf(stderr, "T=1 PDU conflict in descriptor\n");
		return 0;
	}


	trace(cci, " o Max. Message Length: %u bytes\n",
		cci->cci_desc.dwMaxCCIDMessageLength);

	if ( cci->cci_desc.dwFeatures & (CCID_T1_APDU|CCID_T1_APDU_EXT) ) {
		trace(cci, " o APDU Default GetResponse: %u\n",
			cci->cci_desc.bClassGetResponse);
		trace(cci, " o APDU Default Envelope: %u\n",
			cci->cci_desc.bClassEnvelope);
	}

	if ( cci->cci_desc.wLcdLayout ) {
		trace(cci, " o LCD Layout: %ux%u\n",
			bcd_hi(cci->cci_desc.wLcdLayout),
			bcd_lo(cci->cci_desc.wLcdLayout));
	}

	switch ( cci->cci_desc.bPINSupport & (CCID_PIN_VER|CCID_PIN_MOD) ) {
	case CCID_PIN_VER:
		trace(cci, " o PIN verification supported\n");
		break;
	case CCID_PIN_MOD:
		trace(cci, " o PIN modification supported\n");
		break;
	case CCID_PIN_VER|CCID_PIN_MOD:
		trace(cci, " o PIN verification and modification supported\n");
		break;
	}

	_hex_dumpf(cci->cci_tf, ptr, len, 16);
	return 1;
}

static int probe_descriptors(struct _cci *cci)
{
	uint8_t dbuf[512];
	uint8_t *ptr, *end;
	int sz, valid_ccid = 0;

	sz = usb_get_descriptor(cci->cci_dev, USB_DT_CONFIG, 0,
				dbuf, sizeof(dbuf));
	if ( sz < 0 )
		return 0;

	trace(cci, " o Fetching config descriptor\n");

	for(ptr = dbuf, end = ptr + sz; ptr + 2 < end; ) {
		if ( ptr + ptr[0] > end )
			return 0;

		switch( ptr[1] ) {
		case USB_DT_DEVICE:
			break;
		case USB_DT_CONFIG:
			break;
		case USB_DT_STRING:
			break;
		case USB_DT_INTERFACE:
			break;
		case USB_DT_ENDPOINT:
			get_endpoint(cci, ptr, ptr[0]);
			break;
		case CCID_DT:
			if ( fill_ccid_desc(cci, dbuf + 18, ptr[0]) )
				valid_ccid = 1;
			break;
		default:
			trace(cci, " o Unknown descriptor 0x%.2x\n", ptr[1]);
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

/** Connect to a physical chipcard device.
 * \ingroup g_cci
 * @param dev \ref ccidev_t representing a physical device.
 * @param tracefile filename to open for trace logging (or NULL).
 *
 * This function:
 * #- Probes the USB device searching for a valid CCID interface.
 * #- Optionally opens a file for trace logging.
 * #- Perform any device specific initialisation.
 *
 * @return NULL on failure, valid \ref cci_t object otherwise.
 */
cci_t cci_probe(ccidev_t dev, const char *tracefile)
{
	struct _cci *cci = NULL;
	unsigned int x;
	int c, i, a;

	if ( !_probe_device(dev, &c, &i, &a) ) {
		fprintf(stderr, "*** error: failed to probe device %s/%s\n",
			dev->bus->dirname, dev->filename);
		goto out;
	}

	/* First initialize data structures */
	cci = calloc(1, sizeof(*cci));
	if ( NULL == cci )
		goto out;

	if ( tracefile ) {
		if ( !strcmp("-", tracefile) )
			cci->cci_tf = stdout;
		else
			cci->cci_tf = fopen(tracefile, "w");
		if ( cci->cci_tf == NULL )
			goto out_free;
	}

	trace(cci, "Probe CCI on dev %s/%s %d:%d:%d\n",
		dev->bus->dirname, dev->filename, c, i, a);

	for(x = 0; x < CCID_MAX_SLOTS; x++) {
		cci->cci_slot[x].cc_parent = cci;
		cci->cci_slot[x].cc_idx = x;
	}

	/* Second, open USB device and get it ready */
	cci->cci_dev = usb_open(dev);
	if ( NULL == cci->cci_dev ) {
		goto out_free;
	}

#if 1
	usb_reset(cci->cci_dev);
#endif

	if ( usb_set_configuration(cci->cci_dev, c) ) {
		goto out_close;
	}

	if ( usb_claim_interface(cci->cci_dev, i) ) {
		goto out_close;
	}

	if ( usb_set_altinterface(cci->cci_dev, a) ) {
		goto out_close;
	}

	/* Third, probe the CCID class descriptor */
	if( !probe_descriptors(cci) ) {
		goto out_close;
	}

	cci->cci_xfr = _xfr_do_alloc(cci->cci_max_out, cci->cci_max_in);
	if ( NULL == cci->cci_xfr )
		goto out_close;

	/* Fourth, setup each slot */
	for(x = 0; x < cci->cci_num_slots; x++) {
		if ( !_PC_to_RDR_GetSlotStatus(cci, x, cci->cci_xfr) )
			goto out_freebuf;
		if ( !_RDR_to_PC(cci, x, cci->cci_xfr) )
			goto out_freebuf;
		if ( !_RDR_to_PC_SlotStatus(cci, cci->cci_xfr) )
			goto out_freebuf;
	}

	goto out;

out_freebuf:
	_xfr_do_free(cci->cci_xfr);
out_close:
	usb_close(cci->cci_dev);
out_free:
	free(cci);
	cci = NULL;
	fprintf(stderr, "cci: error probing device\n");
out:
	return cci;
}

/** Close connection to a chip card device.
 * \ingroup g_cci
 * @param cci The \ref cci_t to close.
 * Closes connection to physical defice and frees the \ref cci_t. Note that
 * any transaction buffers will have to be destroyed seperately. All references
 * to slots will become invalid.
 */
void cci_close(cci_t cci)
{
	if ( cci ) {
		if ( cci->cci_dev )
			usb_close(cci->cci_dev);
		_xfr_do_free(cci->cci_xfr);
	}
	free(cci);
}

/** Retrieve the number of slots in the CCID.
 * \ingroup g_cci
 * @param cci The \ref cci_t to return number of slots for.
 * @return The number of slots.
 */
unsigned int cci_slots(cci_t cci)
{
	return cci->cci_num_slots;
}

/** Retrieve a handle to a CCID slot.
 * \ingroup g_cci
 * @param cci The \ref cci_t containing the required slot.
 * @param num The slot number to retrieve (zero-based).
 * @return \ref chipcard_t rerpesenting the required slot.
 */
chipcard_t cci_get_slot(cci_t cci, unsigned int num)
{
	if ( num < cci->cci_num_slots ) {
		return cci->cci_slot + num;
	}else{
		return NULL;
	}
}

/** Print a message in the trace log
 * \ingroup g_cci
 * @param cci The \ref cci_t to which the log message is pertinent.
 * @param fmt Format string as per printf().
 *
 * Prints a message in the trace log of the specified CCI device. Does nothing
 * if NULL was passed as tracefile paramater of \ref cci_open.
 */
void cci_log(cci_t cci, const char *fmt, ...)
{
	va_list va;

	if ( NULL == cci->cci_tf )
		return;

	va_start(va, fmt);
	vfprintf(cci->cci_tf, fmt, va);
	va_end(va);
}
