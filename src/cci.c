/*
 * This file is part of cci-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>
#include <ccid-spec.h>

#include <stdio.h>

struct _chipcard {
	struct _cci *cc_parent;
};

struct _cci {
	usb_dev_handle *cci_dev;
	int cci_inp;
	int cci_outp;
	int cci_intrp;
	uint16_t cci_max_in;
	uint16_t cci_max_out;
	unsigned int cci_num_slots;
	unsigned int cci_max_slots;
	struct _chipcard cci_slot[CCID_MAX_SLOTS];
	struct ccid_desc cci_desc;
	uint8_t cci_seq;
	uint8_t *cci_rcvbuf;
	size_t cci_rcvlen;
};

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


static int _RDR_to_PC(struct _cci *cci)
{
	const struct ccid_msg *msg;
	int ret;

	ret = usb_bulk_read(cci->cci_dev, cci->cci_inp,
				(void *)cci->cci_rcvbuf, cci->cci_max_in, 0);
	if ( ret < 0 )
		return 0;

	printf(" Recv: %d bytes", ret);
	cci->cci_rcvlen = (size_t)ret;

	if ( ret < sizeof(*msg) ) {
		printf("\n");
		return 1;
	}

	msg = (struct ccid_msg *)cci->cci_rcvbuf;
	printf(" for slot %u (seq = 0x%.2x)\n",
		msg->bSlot, msg->bSeq);

	switch( msg->in.bStatus & CCID_STATUS_RESULT_MASK ) {
	case CCID_RESULT_SUCCESS:
		printf("     : Command: SUCCESS\n");
		ret = 1;
		break;
	case CCID_RESULT_ERROR:
		printf("     : Command: FAILED (%d)\n", msg->in.bError);
		ret = 0;
		break;
	case CCID_RESULT_TIMEOUT:
		printf("     : Command: Time Extension Request\n");
		ret = 0;
		break;
	default:
		break;
	}

	switch( msg->in.bStatus & CCID_SLOT_STATUS_MASK ) {
	case CCID_STATUS_ICC_ACTIVE:
		printf("     : ICC present and active\n");
		break;
	case CCID_STATUS_ICC_PRESENT:
		printf("     : ICC present and inactive\n");
		break;
	case CCID_STATUS_ICC_NOT_PRESENT:
		printf("     : ICC not presnt\n");
		break;
	default:
		break;
	}

	if ( !ret )
		return 1;

	switch(msg->bMessageType) {
	case RDR_to_PC_DataBlock:
		printf("     : RDR_to_PC_DataBlock\n");
		printf("     : %u bytes extra\n",
			sys_le32(msg->dwLength));
		hex_dump(cci->cci_rcvbuf + 10,
				cci->cci_rcvlen - 10, 16);
		break;
	case RDR_to_PC_SlotStatus:
		printf("     : RDR_to_PC_SlotStatus: ");
		switch(msg->in.bApp) {
		case 0x00:
			printf("Clock running\n");
			break;
		case 0x01:
			printf("Clock stopped in L state\n");
			break;
		case 0x02:
			printf("Clock stopped in H state\n");
			break;
		case 0x03:
			printf("Clock stopped in unknown state\n");
			break;
		default:
			break;
		}
		break;
	case RDR_to_PC_Parameters:
		printf("     : RDR_to_PC_Parameters\n");
		break;
	case RDR_to_PC_Escape:
		printf("     : RDR_to_PC_Escape\n");
		break;
	case RDR_to_PC_BaudAndFreq:
		printf("     : RDR_to_PC_BaudAndFreq\n");
		break;
	default:
		hex_dump(cci->cci_rcvbuf, cci->cci_rcvlen, 16);
		break;
	}

	return 1;
}

static int _PC_to_RDR(struct _cci *cci, unsigned int slot,
			struct ccid_msg *msg, size_t len)
{
	int ret;

	assert(slot < cci->cci_num_slots);

	/* TODO: fill in any extra len in msg->dwLength */
	msg->bSlot = slot;
	msg->bSeq = cci->cci_seq++;

	ret = usb_bulk_write(cci->cci_dev, cci->cci_outp,
				(void *)msg, len, -1);
	if ( ret < 0 )
		return 0;
	if ( (size_t)ret < sizeof(msg) )
		return 0;

	return 1;
}

static int _PC_to_RDR_GetSlotStatus(struct _cci *cci, unsigned int slot)
{
	struct ccid_msg msg;
	int ret;

	memset(&msg, 0, sizeof(msg));
	msg.bMessageType = PC_to_RDR_GetSlotStatus;
	ret = _PC_to_RDR(cci, slot, &msg, sizeof(msg));
	if ( ret )
		printf(" Xmit: PC_to_RDR_GetSlotStatus(%u)\n", slot);
	return ret;
}

static int _PC_to_RDR_IccPowerOn(struct _cci *cci, unsigned int slot)
{
	struct ccid_msg msg;
	int ret;

	memset(&msg, 0, sizeof(msg));
	msg.bMessageType = PC_to_RDR_IccPowerOn;
	ret = _PC_to_RDR(cci, slot, &msg, sizeof(msg));
	if ( ret ) {
		printf(" Xmit: PC_to_RDR_IccPowerOn(%u)\n", slot);
		printf("     : Automatic Voltage Selectio\n");
	}
	return ret;
}

static int get_endpoint(struct _cci *cci, const uint8_t *ptr, size_t len)
{
	const struct usb_endpoint_descriptor *ep;

	if ( len < USB_DT_ENDPOINT_SIZE )
		return 0;

	ep = (const struct usb_endpoint_descriptor *)ptr;

	switch( ep->bmAttributes ) {
	case USB_ENDPOINT_TYPE_BULK:
		printf(" o Bulk %s endpoint at 0x%.2x max=%u bytes\n",
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
		printf(" o Interrupt endpint 0x%.2x\n",
			ep->bEndpointAddress & USB_ENDPOINT_ADDRESS_MASK);
		cci->cci_intrp = ep->bEndpointAddress &
					USB_ENDPOINT_ADDRESS_MASK;
		break;
	default:
		return 0;
	}

	return 1;
}

static int fill_ccid_desc(struct _cci *cci, const uint8_t *ptr, size_t len)
{
	if ( len <  sizeof(cci->cci_desc) ) {
		fprintf(stderr, "*** error: truncated CCID descriptor\n");
		return 0;
	}

	/* FIXME: scan for CCID descriptor */
	memcpy(&cci->cci_desc, ptr, sizeof(cci->cci_desc));

	byteswap_desc(&cci->cci_desc);

	cci->cci_num_slots = cci->cci_desc.bMaxSlotIndex + 1;
	cci->cci_max_slots = cci->cci_desc.bMaxCCIDBusySlots;

	printf(" o got %u/%u byte desc of type 0x%.2x\n",
		len, sizeof(cci->cci_desc), cci->cci_desc.bDescriptorType);
	printf(" o CCID version %u.%u device with %u/%u slots in parallel\n",
			bcd_hi(cci->cci_desc.bcdCCID),
			bcd_lo(cci->cci_desc.bcdCCID),
			cci->cci_max_slots, cci->cci_num_slots);


	printf(" o Voltages:");
	if ( cci->cci_desc.bVoltageSupport & CCID_5V )
		printf(" 5V");
	if ( cci->cci_desc.bVoltageSupport & CCID_3V )
		printf(" 3V");
	if ( cci->cci_desc.bVoltageSupport & CCID_1_8V )
		printf(" 1.8V");
	printf("\n");

	printf(" o Protocols: ");
	if ( cci->cci_desc.dwProtocols & CCID_T0 )
		printf(" T=0");
	if ( cci->cci_desc.dwProtocols & CCID_T1 )
		printf(" T=1");
	printf("\n");

	printf(" o Default Clock Freq.: %uKHz\n", cci->cci_desc.dwDefaultClock);
	printf(" o Max. Clock Freq.: %uKHz\n", cci->cci_desc.dwMaximumClock);
	//printf("   .bNumClocks = %u\n", cci->cci_desc.bNumClockSupported);
	printf(" o Default Data Rate: %ubps\n", cci->cci_desc.dwDataRate);
	printf(" o Max. Data Rate: %ubps\n", cci->cci_desc.dwMaxDataRate);
	//printf("   .bNumDataRates = %u\n", cci->cci_desc.bNumDataRatesSupported);
	printf(" o T=1 Max. IFSD = %u\n", cci->cci_desc.dwMaxIFSD);
	//printf("   .dwSynchProtocols = 0x%.8x\n",
	//	cci->cci_desc.dwSynchProtocols);
	//printf("   .dwMechanical = 0x%.8x\n", cci->cci_desc.dwMechanical);
	
	if ( cci->cci_desc.dwFeatures & CCID_ATR_CONFIG )
		printf(" o Paramaters configured from ATR\n");
	if ( cci->cci_desc.dwFeatures & CCID_ACTIVATE )
		printf(" o Chipcard auto-activate\n");
	if ( cci->cci_desc.dwFeatures & CCID_VOLTAGE )
		printf(" o Auto Voltage Select\n");
	if ( cci->cci_desc.dwFeatures & CCID_FREQ )
		printf(" o Auto Clock Freq. Select\n");
	if ( cci->cci_desc.dwFeatures & CCID_BAUD )
		printf(" o Auto Data Rate Select\n");
	if ( cci->cci_desc.dwFeatures & CCID_CLOCK_STOP )
		printf(" o Clock Stop Mode\n");
	if ( cci->cci_desc.dwFeatures & CCID_NAD )
		printf(" o NAD value other than 0 permitted\n");
	if ( cci->cci_desc.dwFeatures & CCID_IFSD )
		printf(" o Auto IFSD on first exchange\n");
	
	switch ( cci->cci_desc.dwFeatures & (CCID_PPS_VENDOR|CCID_PPS) ) {
	case CCID_PPS_VENDOR:
		printf(" o Proprietary parameter selection algorithm\n");
		break;
	case CCID_PPS:
		printf(" o Chipcard automatic PPD\n");
		break;
	default:
		fprintf(stderr, "PPS/PPS_VENDOR conflict in descriptor\n");
		return 0;
	}

	switch ( cci->cci_desc.dwFeatures &
		(CCID_T1_TPDU|CCID_T1_APDU|CCID_T1_APDU_EXT) ) {
	case CCID_T1_TPDU:
		printf(" o T=1 TPDU\n");
		break;
	case CCID_T1_APDU:
		printf(" o T=1 APDU (Short)\n");
		break;
	case CCID_T1_APDU_EXT:
		printf(" o T=1 APDU (Short and Extended)\n");
		break;
	default:
		fprintf(stderr, "T=1 PDU conflict in descriptor\n");
		return 0;
	}


	printf(" o Max. Message Length: %u bytes\n",
		cci->cci_desc.dwMaxCCIDMessageLength);

	if ( cci->cci_desc.dwFeatures & (CCID_T1_APDU|CCID_T1_APDU_EXT) ) {
		printf(" o APDU Default GetResponse: %u\n",
			cci->cci_desc.bClassGetResponse);
		printf(" o APDU Default Envelope: %u\n",
			cci->cci_desc.bClassEnvelope);
	}

	if ( cci->cci_desc.wLcdLayout ) {
		printf(" o LCD Layout: %ux%u\n",
			bcd_hi(cci->cci_desc.wLcdLayout),
			bcd_lo(cci->cci_desc.wLcdLayout));
	}

	switch ( cci->cci_desc.bPINSupport & (CCID_PIN_VER|CCID_PIN_MOD) ) {
	case CCID_PIN_VER:
		printf(" o PIN verification supported\n");
		break;
	case CCID_PIN_MOD:
		printf(" o PIN modification supported\n");
		break;
	case CCID_PIN_VER|CCID_PIN_MOD:
		printf(" o PIN verification and modification supported\n");
		break;
	}

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

	printf(" o Fetching config descriptor\n");
	hex_dump(dbuf, sz, 16);

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
			printf(" o Unknown descriptor 0x%.2x\n", ptr[1]);
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

cci_t cci_probe(struct usb_device *dev, int c, int i, int a)
{
	struct _cci *cci;
	unsigned int x;

	printf("Probe CCI on dev %s/%s %d:%d:%d\n",
		dev->bus->dirname, dev->filename, c, i, a);

	/* First initialize data structures */
	cci = calloc(1, sizeof(*cci));
	if ( NULL == cci )
		goto out;

	for(x = 0; x < CCID_MAX_SLOTS; x++) {
		cci->cci_slot[x].cc_parent = cci;
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

	cci->cci_rcvbuf = calloc(1, cci->cci_max_in);
	if ( NULL == cci->cci_rcvbuf ) {
		goto out_close;
	}

	/* TODO: Fourth, setup each slot */
	for(x = 0; x < cci->cci_num_slots; x++) {
		if ( !_PC_to_RDR_GetSlotStatus(cci, x) )
			goto out_freebuf;
		if ( !_RDR_to_PC(cci) )
			goto out_freebuf;
		if ( !_PC_to_RDR_IccPowerOn(cci, x) )
			goto out_freebuf;
		if ( !_RDR_to_PC(cci) )
			goto out_freebuf;
	}

	goto out;

out_freebuf:
	free(cci->cci_rcvbuf);
out_close:
	usb_close(cci->cci_dev);
out_free:
	free(cci);
	cci = NULL;
out:
	return cci;
}

void cci_close(cci_t cci)
{
	if ( cci ) {
		if ( cci->cci_dev )
			usb_close(cci->cci_dev);
		free(cci->cci_rcvbuf);
	}
	free(cci);
}
