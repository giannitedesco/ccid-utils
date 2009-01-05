/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/
#ifndef _CCID_SPEC_H
#define _CCID_SPEC_H

/* CCID Class Descriptor S.3.5 */
struct ccid_desc {
	uint8_t		bLength;
	uint8_t		bDescriptorType;
	uint16_t	bcdCCID;
	uint8_t		bMaxSlotIndex;

#define CCID_5V		(1<<0)
#define CCID_3V		(1<<1)
#define CCID_1_8V	(1<<2)
	uint8_t		bVoltageSupport;

#define CCID_T0		(1<<0)
#define CCID_T1		(1<<1)
	uint32_t	dwProtocols;
	uint32_t	dwDefaultClock;
	uint32_t	dwMaximumClock;
	uint8_t		bNumClockSupported;
	uint32_t	dwDataRate;
	uint32_t	dwMaxDataRate;
	uint8_t		bNumDataRatesSupported;
	uint32_t	dwMaxIFSD;

#define CCID_2WIRE	(1<<0)
#define CCID_3WIRE	(1<<1)
#define CCID_I2C	(1<<2)
	uint32_t	dwSynchProtocols;
	uint32_t	dwMechanical;

#define CCID_ATR_CONFIG		(1<<1)
#define CCID_ACTIVATE		(1<<2)
#define CCID_VOLTAGE		(1<<3)
#define CCID_FREQ		(1<<4)
#define CCID_BAUD		(1<<5)
#define CCID_PPS_VENDOR		(1<<6)
#define CCID_PPS		(1<<7)
#define CCID_CLOCK_STOP		(1<<8)
#define CCID_NAD		(1<<9)
#define CCID_IFSD		(1<<10)

#define CCID_T1_TPDU		(1<<16)
#define CCID_T1_APDU		(1<<17)
#define CCID_T1_APDU_EXT	(1<<18)
	uint32_t	dwFeatures;
	uint32_t	dwMaxCCIDMessageLength;
	uint8_t		bClassGetResponse;
	uint8_t		bClassEnvelope;
	uint16_t	wLcdLayout;

#define CCID_PIN_VER	(1<<0)
#define CCID_PIN_MOD	(1<<1)
	uint8_t		bPINSupport;
	uint8_t		bMaxCCIDBusySlots;
} _packed;

/* CCID Message Header */
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
} _packed;

#endif /* _CCID_SPEC_H */
