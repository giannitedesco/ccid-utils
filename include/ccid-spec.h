/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/
#ifndef _CCID_SPEC_H
#define _CCID_SPEC_H

/* Descriptor type */
#define CCID_DT				0x21U

/* Max slots */
#define CCID_MAX_SLOTS			0x10U

#define CCID_ERR_ABORT			0xff
#define CCID_ERR_MUTE			0xfe
#define CCID_ERR_PARITY			0xfd
#define CCID_ERR_OVERRUN		0xfc
#define CCID_ERR_HARDWARE		0xfb
#define CCID_ERR_BAD_TS			0xf8
#define CCID_ERR_BAD_TCK		0xf7
#define CCID_ERR_PROTOCOL		0xf6
#define CCID_ERR_CLASS			0xf5
#define CCID_ERR_PROCEDURE		0xf4
#define CCID_ERR_DEACTIVATED		0xf3
#define CCID_ERR_AUTO_SEQ		0xf2
#define CCID_ERR_PIN_TIMEOUT		0xf0
#define CCID_ERR_BUSY			0xe0
#define CCID_ERR_USR_MIN		0x81
#define CCID_ERR_USR_MAX		0xc0

/* Yes of course, all the world is a PC.... (or a RDR) */
/* Bulk IN */
#define RDR_to_PC_DataBlock		0x80
#define RDR_to_PC_SlotStatus		0x81
#define RDR_to_PC_Parameters		0x82
#define RDR_to_PC_Escape		0x83
#define RDR_to_PC_BaudAndFreq		0x84

/* Interrupt */
#define RDR_to_PC_NotifySlotChange	0x50
#define RDR_to_PC_HardwareError		0x51

/* Bulk Out */
#define PC_to_RDR_SetParameters		0x61
#define PC_to_RDR_IccPowerOn		0x62
#define PC_to_RDR_IccPowerOff		0x63
/* 64 */
#define PC_to_RDR_GetSlotStatus		0x65
/* 67,68 */
#define PC_to_RDR_Secure		0x69
#define PC_to_RDR_T0APDU		0x6a
#define PC_to_RDR_Escape		0x6b
#define PC_to_RDR_GetParameters		0x6c
#define PC_to_RDR_ResetParameters	0x6d
#define PC_to_RDR_IccClock		0x6e
#define PC_to_RDR_XfrBlock		0x6f
#define PC_to_RDR_Mechanical		0x71
#define PC_to_RDR_Abort			0x72
#define PC_to_RDR_SetBaudAndFreq	0x73

#define CCID_STATUS_RESULT_MASK		0xc0
#define CCID_RESULT_SUCCESS		(0 << 6)
#define CCID_RESULT_ERROR		(1 << 6)
#define CCID_RESULT_TIMEOUT		(2 << 6)

#define CCID_SLOT_STATUS_MASK		0x03
#define CCID_STATUS_ICC_ACTIVE		0x0
#define CCID_STATUS_ICC_PRESENT		0x1
#define CCID_STATUS_ICC_NOT_PRESENT	0x2

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

#endif /* _CCID_SPEC_H */
