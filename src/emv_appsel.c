/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>
#include <list.h>
#include <emv.h>
#include "emv-internal.h"

static int add_app(emv_t e)
{
	struct _emv_pse *pse;
	struct pse_fci *pse_fci;
	const uint8_t *res;
	size_t len;

	res = xfr_rx_data(e->e_xfr, &len);
	if ( NULL == res )
		return 0;

	pse_fci = pse_fci_decode(res, len);
	if ( NULL == pse_fci ) {
		_emv_error(e, EMV_ERR_DATA_ELEMENT_NOT_FOUND);
		return 0;
	}

	pse = malloc(sizeof(*pse));
	if ( NULL == pse ) {
		_emv_sys_error(e);
		pse_fci_free(pse_fci);
		return 0;
	}

	pse->p_fci = pse_fci;
	list_add_tail(&pse->p_list, &e->e_pse);
	return 1;
}

void _emv_free_applist(emv_t e)
{
	struct _emv_pse *pse, *t;
	list_for_each_entry_safe(pse, t, &e->e_pse, p_list) {
		pse_fci_free(pse->p_fci);
		list_del(&pse->p_list);
		free(pse);
	}
}

void emv_pse_rid(emv_pse_t pse, emv_rid_t ret)
{
	memcpy(ret, pse->p_fci->pse_app.pse_adf_name, EMV_RID_LEN);
}

void emv_pse_aid(emv_pse_t pse, uint8_t *ret, size_t *len)
{
	memcpy(ret, pse->p_fci->pse_app.pse_adf_name,
		pse->p_fci->pse_app._pse_adf_name_count);
	*len = pse->p_fci->pse_app._pse_adf_name_count;
}

const char *emv_pse_label(emv_pse_t pse)
{
	/* FIXME: may not be nul terminated */
	return (char *)pse->p_fci->pse_app.pse_label;
}

const char *emv_pse_pname(emv_pse_t pse)
{
	if ( pse->p_fci->pse_app._present & PSE_APP_PSE_PNAME )
		return (char *)pse->p_fci->pse_app.pse_pname;
	else
		return NULL;
}

uint8_t emv_pse_prio(emv_pse_t pse)
{
	return pse->p_fci->pse_app.pse_prio & 0x7f;
}

int emv_pse_confirm(emv_pse_t pse)
{
	return pse->p_fci->pse_app.pse_prio >> 7;
}

int emv_appsel_psd(emv_t e)
{
	static const char * const psd = "1PAY.SYS.DDF01";
	struct _emv_pse *pse, *tmp;
	unsigned int i;

	if ( !_emv_select(e, (uint8_t *)psd, strlen(psd)) )
		return 0;

	list_for_each_entry_safe(pse, tmp, &e->e_pse, p_list) {
		pse_fci_free(pse->p_fci);
		list_del(&pse->p_list);
		free(pse);
	}

	for (i = 1; ; i++) {
		if ( !_emv_read_record(e, 1, i) )
			break;
		add_app(e);
	}

	/* TODO: Sort by priority */

	_emv_success(e);
	return 1;
}

emv_pse_t emv_appsel_pse_first(emv_t e)
{
	if ( list_empty(&e->e_pse) )
		return NULL;
	return list_entry(e->e_pse.next, struct _emv_pse, p_list);
}

emv_pse_t emv_appsel_pse_next(emv_t e, emv_pse_t app)
{
	if ( app->p_list.next == &e->e_pse )
		return NULL;
	return list_entry(app->p_list.next, struct _emv_pse, p_list);
}

_public void emv_pse_delete(emv_pse_t pse)
{
	pse_fci_free(pse->p_fci);
	list_del(&pse->p_list);
	free(pse);
}

static int set_app(emv_t e)
{
	struct adf_fci *adf_fci;
	const uint8_t *fci;
	size_t len;

	fci = xfr_rx_data(e->e_xfr, &len);
	if ( NULL == fci )
		return 0;

	adf_fci = adf_fci_decode(fci, len);
	if ( NULL == adf_fci ) {
		_emv_error(e, EMV_ERR_DATA_ELEMENT_NOT_FOUND);
		return 0;
	}

	if ( e->e_app_fci )
		adf_fci_free(e->e_app_fci);

	e->e_app_fci = adf_fci;
	return 0;
}

int emv_app_select_pse(emv_t e, emv_pse_t pse)
{
	if ( !_emv_select(e, pse->p_fci->pse_app.pse_adf_name,
		pse->p_fci->pse_app._pse_adf_name_count) )
		return 0;
	return set_app(e);
}

int emv_app_select_aid(emv_t e, const uint8_t *aid, size_t aid_len)
{
	if ( !_emv_select(e, aid, aid_len) )
		return 0;
	return set_app(e);
}

int emv_app_select_aid_next(emv_t e, const uint8_t *aid, size_t aid_len)
{
	if ( !_emv_select_next(e, aid, aid_len) )
		return 0;
	return set_app(e);
}
