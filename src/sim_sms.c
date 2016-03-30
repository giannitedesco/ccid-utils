/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>
#include "sim-internal.h"

static void decode_7bit(const uint8_t *inp, size_t len)
{
	uint8_t out[len + 1];
	unsigned int i;

	for(i = 0; i <= len; i++) {
		int ipos = i - (i >> 3);
		int offset = (i & 0x7);

		out[i] = (inp[ipos] & (0x7F >> offset)) << offset;
		if( 0 == offset )
			continue;

		out[i] |= (inp[ipos - 1] & 
			(0x7F << (8 - offset)) & 0xFF) >> (8 - offset);
	}

	out[len] = '\0';
	printf(" \"%s\"\n", out);
}

static uint8_t hi_nibble(uint8_t byte)
{
	return byte >> 4;
}

static uint8_t lo_nibble(uint8_t byte)
{
	return byte & 0xf;
}

const char *number_type(uint8_t type)
{
	switch(type & GSM_NUMBER_TYPE_MASK) {
	case GSM_NUMBER_UNKNOWN:
		return "unknown";
	case GSM_NUMBER_INTL:
		return "international";
	case GSM_NUMBER_NATIONAL:
		return "national";
	case GSM_NUMBER_NET_SPEC:
		return "network specific";
	case GSM_NUMBER_SUBSCR:
		return "subscriber";
	case GSM_NUMBER_ABBREV:
		return "abbreviated";
	case GSM_NUMBER_ALNUM:
		return "alphanumeric";
	case GSM_NUMBER_RESERVED:
	default:
		return "reserved";
	}
}
const char *number_plan(uint8_t type)
{
	switch(type & GSM_PLAN_MASK) {
	case GSM_PLAN_UNKNOWN:
		return "unknown";
	case GSM_PLAN_ISDN:
		return "ISDN";
	case GSM_PLAN_X121:
		return "X.121";
	case GSM_PLAN_TELEX:
		return "telex";
	case GSM_PLAN_NATIONAL:
		return "national";
	case GSM_PLAN_PRIVATE:
		return "private";
	case GSM_PLAN_ERMES:
		return "ERMES";
	default:
		return "reserved";
	}
}

const char *fmt_number(uint8_t type, uint8_t len, const uint8_t *ptr)
{
	static char buf[768];
	char *o = buf;
	uint8_t i;

	if ( GSM_NUMBER_INTL == (type & GSM_NUMBER_TYPE_MASK) )
		*o = '+', o++;
	
	for(i = 0; i < len; i++, ptr++) {
		if ( *ptr == 0xff )
			continue;
		switch(lo_nibble(*ptr)) {
		case 0 ... 9:
			o += sprintf(o, "%1x", lo_nibble(*ptr));
			break;
		case 0xa:
			*o = '*', o++;
			break;
		case 0xb:
			*o = '#', o++;
			break;
		default:
			break;
		}
		if ( hi_nibble(*ptr) != 0xf )
			o += sprintf(o, "%1x", hi_nibble(*ptr));
		if ( 0 == i || 1 == ((i - 1) % 2) )
			*o = ' ', o++;
	}
	*o = '\0';
	return buf;
}

void _sms_decode(struct _sms *sms, const uint8_t *ptr)
{
	//const uint8_t *end = ptr + 176;

	memset(sms, 0, sizeof(*sms));

	sms->status = *ptr, ptr++;

	switch( sms->status & 0x7 ) {
	case SIM_SMS_STATUS_FREE:
		/* free block */
		return;
	case SIM_SMS_STATUS_READ:
		printf("sms: Status: READ\n");
		break;
	case SIM_SMS_STATUS_UNREAD:
		printf("sms: Status: UNREAD\n");
		break;
	case SIM_SMS_STATUS_SENT:
		printf("sms: Status: SENT\n");
		break;
	case SIM_SMS_STATUS_UNSENT:
		printf("sms: Status: UNSENT\n");
		break;
	default:
		printf("sms: Status: unknown status 0x%.1x\n",
			sms->status & 0x7);
		return;
	}

	sms->smsc_len = ptr[0] - 1;
	sms->smsc_type = ptr[1];
	ptr += 2;
	sms->smsc = ptr;
	ptr += sms->smsc_len;
	printf(" SMSC: type %s/%s\n",
		number_type(sms->smsc_type),
		number_plan(sms->smsc_type));
	printf(" SMSC: %s\n",
		fmt_number(sms->smsc_type, sms->smsc_len, sms->smsc));

	sms->sms_deliver = *ptr, ptr++;
	if ( (sms->sms_deliver & SMS_TP_MTI) ) {
		printf(" Not an SMS-DELIVER 0x%.2x\n", sms->sms_deliver);
		return;
	}
	printf(" SMS-DELIVER");
	if ( 0 == (sms->sms_deliver & SMS_TP_MMS) )
		printf(" MMS");
	if ( sms->sms_deliver & SMS_TP_SRI )
		printf(" SRI");
	if ( sms->sms_deliver & SMS_TP_UDHI )
		printf(" UDHI");
	if ( sms->sms_deliver & SMS_TP_RP )
		printf(" RP");
	printf("\n");

	sms->sender_len = (ptr[0] + 1) >> 1;
	sms->sender_type = ptr[1];
	ptr += 2;
	sms->sender = ptr;
	ptr += sms->sender_len;
	printf(" Sender: type %s/%s\n",
		number_type(sms->sender_type),
		number_plan(sms->sender_type));
	printf(" Sender: %s\n",
		fmt_number(sms->sender_type, sms->sender_len, sms->sender));

	sms->tp_pid = ptr[0];
	sms->tp_dcs = ptr[1];
	ptr += 2;
	printf(" TP-PID = 0x%.2x, TP-DCS = 0x%.2x\n", sms->tp_pid, sms->tp_dcs);

	memcpy(sms->timestamp, ptr, 7), ptr += 7;
	printf(" Timestamp: 20%1x%1x-%1x%1x-%1x%1x %1x%1x:%1x%1x:%1x%1x\n",
		lo_nibble(sms->timestamp[0]),
		hi_nibble(sms->timestamp[0]),
		lo_nibble(sms->timestamp[1]),
		hi_nibble(sms->timestamp[1]),
		lo_nibble(sms->timestamp[2]),
		hi_nibble(sms->timestamp[2]),
		lo_nibble(sms->timestamp[3]),
		hi_nibble(sms->timestamp[3]),
		lo_nibble(sms->timestamp[4]),
		hi_nibble(sms->timestamp[4]),
		lo_nibble(sms->timestamp[5]),
		hi_nibble(sms->timestamp[5]));

	sms->uda = ptr[0], ptr++;
	sms->data = ptr;
	decode_7bit(sms->data, sms->uda);
	printf("\n");
}
