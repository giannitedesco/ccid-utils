/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/
#ifndef _CCID_INTERNAL_H
#define _CCID_INTERNAL_H

struct _chipcard {
	struct _cci *cc_parent;
	uint8_t cc_idx;
	uint8_t cc_status;
};

struct _cci {
	usb_dev_handle *cci_dev;

	uint8_t 	*cci_rcvbuf;
	size_t 		cci_rcvlen;
	size_t		cci_rcvmax;

	int 		cci_inp;
	int 		cci_outp;
	int 		cci_intrp;

	uint16_t 	cci_max_in;
	uint16_t 	cci_max_out;
	uint16_t 	cci_max_intr;

	uint8_t 	cci_seq;
	uint8_t		_pad0;

	unsigned int 	cci_num_slots;
	unsigned int 	cci_max_slots;
	struct _chipcard cci_slot[CCID_MAX_SLOTS];

	struct ccid_desc cci_desc;
};

int _cci_get_cmd_result(const struct ccid_msg *msg, int *code);

const struct ccid_msg *_RDR_to_PC(struct _cci *cci);
unsigned int _RDR_to_PC_SlotStatus(const struct ccid_msg *msg);
unsigned int _RDR_to_PC_DataBlock(const struct ccid_msg *msg);

int _PC_to_RDR_GetSlotStatus(struct _cci *cci, unsigned int slot);
int _PC_to_RDR_IccPowerOn(struct _cci *cci, unsigned int slot,
				unsigned int voltage);
int _PC_to_RDR_IccPowerOff(struct _cci *cci, unsigned int slot);
int _PC_to_RDR_XfrBlock(struct _cci *cci, unsigned int slot,
				const uint8_t *ptr, size_t len);

void _chipcard_set_status(struct _chipcard *cc, unsigned int status);

int _cci_wait_for_interrupt(struct _cci *cci);

#endif /* _CCID_INTERNAL_H */
