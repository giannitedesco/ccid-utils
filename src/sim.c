/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>
#include <sim.h>
#include "sim-internal.h"

sim_t sim_new(chipcard_t cc)
{
	struct _sim *sim;
	const uint8_t *atr;
	size_t atr_len;

	sim = calloc(1, sizeof(*sim));
	if ( NULL == sim )
		goto err;

	sim->s_cc = cc;

	sim->s_xfr = xfr_alloc(502, 502);
	if ( NULL == sim->s_xfr )
		goto err_free;

	atr = chipcard_slot_on(sim->s_cc, CHIPCARD_AUTO_VOLTAGE, &atr_len);
	if ( NULL == atr )
		goto err_free_xfr;

	printf(" o Found SIM with %u byte ATR:\n", atr_len);
	hex_dump(atr, atr_len, 16);

	if ( !_sim_select(sim, SIM_MF) )
		goto err_free_xfr;

	_sim_select(sim, SIM_EF_ICCID);

	return sim;

err_free_xfr:
	xfr_free(sim->s_xfr);
err_free:
	free(sim);
err:
	return NULL;
}

void sim_free(sim_t sim)
{
	if ( sim ) {
		if ( sim->s_xfr )
			xfr_free(sim->s_xfr);
		if ( sim->s_cc )
			chipcard_slot_off(sim->s_cc);
	}
	free(sim);
}
