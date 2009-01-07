/*
 * This file is part of cci-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>
#include <ccid-spec.h>

#include <stdio.h>

#include "ccid-internal.h"

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
		printf(" o Interrupt %s endpint 0x%.2x\n",
			(ep->bEndpointAddress &  USB_ENDPOINT_DIR_MASK) ?
				"IN" : "OUT",
			ep->bEndpointAddress & USB_ENDPOINT_ADDRESS_MASK);
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

	cci->cci_rcvbuf = calloc(1, (cci->cci_max_in > cci->cci_max_intr) ? 
					cci->cci_max_in : cci->cci_max_intr);
	if ( NULL == cci->cci_rcvbuf ) {
		goto out_close;
	}

	/* Fourth, setup each slot */
	for(x = 0; x < cci->cci_num_slots; x++) {
		const struct ccid_msg *msg;

		if ( !_PC_to_RDR_GetSlotStatus(cci, x) )
			goto out_freebuf;
		msg = _RDR_to_PC(cci);
		if ( NULL == msg)
			goto out_freebuf;
		_RDR_to_PC_SlotStatus(msg);
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

unsigned int cci_slots(cci_t cci)
{
	return cci->cci_num_slots;
}

chipcard_t cci_get_slot(cci_t cci, unsigned int i)
{
	if ( i < cci->cci_num_slots ) {
		return cci->cci_slot + i;
	}else{
		return NULL;
	}
}
