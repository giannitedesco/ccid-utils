/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/
#ifndef _EMV_INTERNAL_H
#define _EMV_INTERNAL_H

struct _emv_app {
	uint8_t a_recno;
	uint8_t a_id_sz;
	/* uint16_t a_pad0; */
	uint8_t a_id[16];
	char a_name[16];
	char a_pname[16];
	struct list_head a_list;
};

struct _emv {
	chipcard_t e_dev;
	xfr_t e_xfr;
	unsigned int e_num_apps;
	struct list_head e_apps;
	struct _emv_app *e_cur;
	union {
		struct {
		}a_link;
		struct {
		}a_visa;
	}e_u;
};

#endif /* _EMV_INTERNAL_H */
