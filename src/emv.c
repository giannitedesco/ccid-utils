/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>
#include <list.h>
#include <emv.h>
#include <ber.h>
#include "emv-internal.h"

static void do_emv_fini(emv_t e)
{
	if ( e ) {
 		free(e->e_sda.iss_cert);
 		free(e->e_sda.iss_exp);
 		free(e->e_sda.iss_pubkey_r);
 		free(e->e_sda.ssa_data);
		RSA_free(e->e_sda.iss_pubkey);

		_emv_free_applist(e);
 
		if ( e->e_xfr )
			xfr_free(e->e_xfr);
		free(e);
	}
}

emv_t emv_init(chipcard_t cc)
{
	struct _emv *e;

	if ( chipcard_status(cc) != CHIPCARD_ACTIVE )
		return NULL;

	e = calloc(1, sizeof(*e));
	if ( e ) {
		e->e_dev = cc;
		INIT_LIST_HEAD(&e->e_apps);

		e->e_xfr = xfr_alloc(1024, 1204);
		if ( NULL == e->e_xfr )
			goto err;

		_emv_init_applist(e);
	}

	return e;

err:
	do_emv_fini(e);
	return NULL;
}

void emv_fini(emv_t e)
{
	do_emv_fini(e);
}
