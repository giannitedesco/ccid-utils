/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>
#include <errno.h>
#include <list.h>
#include <emv.h>
#include <ber.h>
#include "emv-internal.h"

void _emv_sys_error(struct _emv *e)
{
	e->e_err = (EMV_ERR_SYSTEM << EMV_ERR_TYPE_SHIFT) | 
			(errno & EMV_ERR_CODE_MASK);
}

void _emv_ccid_error(struct _emv *e)
{
	e->e_err = (EMV_ERR_CCID << EMV_ERR_TYPE_SHIFT);
}

void _emv_icc_error(struct _emv *e)
{
	e->e_err = (EMV_ERR_ICC << EMV_ERR_TYPE_SHIFT) | 
			(_emv_sw1(e) << 8) |
			(_emv_sw2(e) << 8);
}

void _emv_error(struct _emv *e, unsigned int code)
{
	e->e_err = (EMV_ERR_EMV << EMV_ERR_TYPE_SHIFT) | 
			(code & EMV_ERR_CODE_MASK);
}

emv_err_t emv_error(emv_t e)
{
	return e->e_err;
}

void _emv_success(struct _emv *e)
{
	e->e_err = 0;
}

unsigned int emv_error_type(emv_err_t e)
{
	return e >> EMV_ERR_TYPE_SHIFT;
}

unsigned int emv_error_additional(emv_err_t e)
{
	return e & EMV_ERR_CODE_MASK;
}

static const struct {
	uint16_t code;
	const char * const str;
}icc_errs[] = {
	{0x6283, "Selected file not found"},
	{0x6985, "Conditions of use not satisfied"},
	{0x6a81, "Selected function not supported"},
	{0x6a83, "Record not found"},
	{0x6a84, "Not enough memory space in file"},
	{0x6a85, "Lc inconsistent with TLV structure"},
	{0x6a88, "Referenced data not found"},
};
static const char *icc_err_string(uint32_t code)
{
	unsigned int i;
	for(i = 0; i < sizeof(icc_errs)/sizeof(icc_errs); i++)
		if ( icc_errs[i].code == code )
			return icc_errs[i].str;
	return NULL;
}

static const char *err_string(uint32_t code)
{
	return "oops";
}

const char *emv_error_string(emv_err_t err)
{
	switch ( err >> EMV_ERR_TYPE_SHIFT ) {
	case EMV_ERR_SYSTEM:
		return strerror(err & EMV_ERR_CODE_MASK);
	case EMV_ERR_CCID:
		return "Communication with ICC interrupted";
	case EMV_ERR_ICC:
		return icc_err_string(err & EMV_ERR_CODE_MASK);
	case EMV_ERR_EMV:
		return err_string(err & EMV_ERR_CODE_MASK);
	default:
		return "Error in error code...heh";
	}
}
