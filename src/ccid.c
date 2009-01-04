/*
 * This file is part of ccid 
 * Copyright (c) 2006 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 2
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <usb.h>

struct ccid_desc {
	uint8_t		bLength;
	uint8_t		bDescriptorType;
	uint16_t	bcdCCID;
	uint8_t		bMaxSlotIndex;
	uint8_t		bVoltageSupport;
	uint32_t	dwProtocols;
	uint32_t	dwDefaultClock;
	uint32_t	dwMaximumClock;
	uint8_t		bNumClockSupported;
	uint32_t	dwDataRate;
	uint32_t	dwMaxDataRate;
	uint8_t		bNumDataRatesSupported;
	uint32_t	dwMaxIFSD;
	uint32_t	dwSynchProtocols;
	uint32_t	dwMechanical;
	uint32_t	dwFeatures;
	uint32_t	dwMaxCCIDMessageLength;
	uint8_t		bClassGetResponse;
	uint8_t		bClassEnvelope;
	uint16_t	wLcdLayout;
	uint8_t		bPINSupport;
	uint8_t		bMaxCCIDBusySlots;
}__attribute__((packed));

struct ccid_msg {
	uint8_t		bMessageType;
	uint32_t 	dwLength;
	uint8_t		bSlot;
	uint8_t		bSeq;
	union {
		struct {
			uint8_t	bStatus;
			uint8_t	bError;
			uint8_t	bApp;
		}in;
		struct {
			uint8_t	bApp[3];
		}out;
	};
}__attribute__((packed));

struct ccid_dev {
	usb_dev_handle *dev;
	int inp, outp, intp;
	uint8_t seq;
	struct ccid_desc desc;
};

static void hex_dump(void *t, size_t len, size_t llen)
{
	uint8_t *tmp = t;
	size_t i, j;
	size_t line;

	for(j=0; j < len; j += line, tmp += line) {
		if ( j + llen > len ) {
			line = len - j;
		}else{
			line = llen;
		}

		printf(" | %05X : ", j);

		for(i=0; i < line; i++) {
			if ( isprint(tmp[i]) ) {
				printf("%c", tmp[i]);
			}else{
				printf(".");
			}
		}

		for(; i < llen; i++)
			printf(" ");

		for(i = 0; i < line; i++)
			printf(" %02X", tmp[i]);

		printf("\n");
	}
	printf("\n");
}

static int fill_ccid_desc(struct ccid_dev *ccid)
{
	struct {
		uint8_t pad[18];
		struct ccid_desc desc;
	}__attribute__((packed)) dbuf;
	int sz;

	sz = usb_get_descriptor(ccid->dev, 0x2, 0, &dbuf, sizeof(dbuf));
	if ( sz < 0 )
		return 0;

	memcpy(&ccid->desc, &dbuf.desc, sizeof(ccid->desc));

	printf(" o got %u/%u byte desc of type 0x%.2x\n",
		sz, sizeof(dbuf), dbuf.desc.bDescriptorType);
	printf("   .bcdCCID = 0x%.4x\n", dbuf.desc.bcdCCID);
	printf("   .bMaxSlotIndex = %u\n", dbuf.desc.bMaxSlotIndex);
	printf("   .bVoltateSupport = 0x%.2x\n", dbuf.desc.bVoltageSupport);
	printf("   .dwProtocols = 0x%.4x\n", dbuf.desc.dwProtocols & 0xffff);
	printf("   .dwDefaultClock = %uKHz\n", dbuf.desc.dwDefaultClock);
	printf("   .dwMaximumClock = %uKHz\n", dbuf.desc.dwMaximumClock);
	printf("   .bNumClocks = %u\n", dbuf.desc.bNumClockSupported);
	printf("   .dwDataRate = %ubps\n", dbuf.desc.dwDataRate);
	printf("   .dwMaxDataRate = %ubps\n", dbuf.desc.dwMaxDataRate);
	printf("   .bNumDataRates = %u\n", dbuf.desc.bNumDataRatesSupported);
	printf("   .dwMaxIFSD = %u\n", dbuf.desc.dwMaxIFSD);
	printf("   .dwSynchProtocols = 0x%.4x\n",
		dbuf.desc.dwSynchProtocols & 0xffff);
	printf("   .dwMechanical = 0x%.8x\n", dbuf.desc.dwMechanical);
	printf("   .dwFeatures = 0x%.8x\n", dbuf.desc.dwFeatures);
	printf("   .dwMaxCCIDLen = %u\n", dbuf.desc.dwMaxCCIDMessageLength);
	printf("   .bClassGetResponse = %u\n", dbuf.desc.bClassGetResponse);
	printf("   .bClassEnvelope = %u\n", dbuf.desc.bClassEnvelope);
	printf("   .wLcdLayout = %ux%u\n",
		(dbuf.desc.wLcdLayout & 0xff00) >> 8,
		dbuf.desc.wLcdLayout & 0xff);
	printf("   .bPinSupport = 0x%.2x\n", dbuf.desc.bPINSupport);
	printf("   .bMaxCCIDBusySlots = %u\n", dbuf.desc.bMaxCCIDBusySlots);

	return 1;
}

static int found_cci(struct usb_device *dev, int c, int i, int a)
{
	struct ccid_dev ccid;

	printf("Found CCI on dev %s/%s %u:%u:%u\n",
		dev->bus->dirname, dev->filename, c, i, a);

	ccid.dev = usb_open(dev);
	if ( ccid.dev == NULL ) {
		return 0;
	}

#if 0
	usb_reset(ccid.dev);
#endif

	printf(" o device opened\n");

	if ( usb_set_configuration(ccid.dev, c) )
		return 0;

	printf(" o set configuration\n");

	if ( usb_claim_interface(ccid.dev, i) )
		return 0;

	printf(" o claimed interface\n");

	if ( usb_set_altinterface(ccid.dev, a) )
		return 0;

	printf(" o set alternate interface\n");

	if( !fill_ccid_desc(&ccid) )
		return 0;

	usb_close(ccid.dev);
	return 1;
}

static int check_interface(struct usb_device *dev, int c, int i)
{
	struct usb_interface *iface = &dev->config[c].interface[i];
	int a;

	for (a = 0; a < iface->num_altsetting; a++) {
		struct usb_interface_descriptor *id = &iface->altsetting[a];
		if ( id->bInterfaceClass == 0x0b ) {
			if ( found_cci(dev, c, i, a) )
				return 1;
		}
	}

	return 0;
}
static void check_device(struct usb_device *dev)
{
	int c, i;

	if ( dev->descriptor.idVendor == 0x4e6 &&
		dev->descriptor.idProduct == 0xe003 ) {
		found_cci(dev, 1, 0, 0);
		return;
	}

	for(c = 0; c < dev->descriptor.bNumConfigurations; c++) {
		for(i = 0; i < dev->config[c].bNumInterfaces; i++) {
			if ( check_interface(dev, c, i) )
				return;
		}
	}
}

int main(int argc, char **argv)
{
	struct usb_bus *bus, *busses;
	struct usb_device *dev;

	usb_init();
	usb_find_busses();
	usb_find_devices();

	busses = usb_get_busses();

	for(bus = busses; bus; bus = bus->next) {
		for(dev = bus->devices; dev; dev = dev->next)
			check_device(dev);
	}

	return 0;
}
