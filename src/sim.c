/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>
#include "sim.h"

struct _sim {
	chipcard_t	s_cc;
	xfr_t		s_xfr;
	uint16_t	s_pwd;
	uint8_t		s_reclen;
};

static int do_read_record(sim_t sim, uint8_t rec, uint8_t le)
{
	printf(" o Trying to READ_RECORD %u (%u bytes)\n", rec, le);
	xfr_reset(sim->s_xfr);
	xfr_tx_byte(sim->s_xfr, SIM_CLA);
	xfr_tx_byte(sim->s_xfr, SIM_INS_READ_RECORD);
	xfr_tx_byte(sim->s_xfr, rec); /* p1 */
	xfr_tx_byte(sim->s_xfr, 4); /* p2 */
	xfr_tx_byte(sim->s_xfr, le);
	return chipcard_transact(sim->s_cc, sim->s_xfr);
}

static int do_select(sim_t sim, uint16_t id)
{
	printf(" o Trying to SELECT_FILE 0x%.4x\n", id);
	xfr_reset(sim->s_xfr);
	xfr_tx_byte(sim->s_xfr, SIM_CLA);
	xfr_tx_byte(sim->s_xfr, SIM_INS_SELECT);
	xfr_tx_byte(sim->s_xfr, 0); /* p1 */
	xfr_tx_byte(sim->s_xfr, 0); /* p2 */
	xfr_tx_byte(sim->s_xfr, 2); /* lc */
	xfr_tx_byte(sim->s_xfr, (id >> 8));
	xfr_tx_byte(sim->s_xfr, (id & 0xff));
	return chipcard_transact(sim->s_cc, sim->s_xfr);
}

static int do_get_response(sim_t sim, uint8_t le)
{
	printf(" o Trying to GET_RESPONSE %u bytes\n", le);
	xfr_reset(sim->s_xfr);
	xfr_tx_byte(sim->s_xfr, SIM_CLA);
	xfr_tx_byte(sim->s_xfr, SIM_INS_GET_RESPONSE);
	xfr_tx_byte(sim->s_xfr, 0); /* p1 */
	xfr_tx_byte(sim->s_xfr, 0); /* p2 */
	xfr_tx_byte(sim->s_xfr, le);
	return chipcard_transact(sim->s_cc, sim->s_xfr);
}

static int do_fci_ef(struct _sim *sim, const uint8_t *fci, size_t len)
{
	assert(len == 15);
	sim->s_reclen = fci[14];
	printf(" o Record len %u\n", sim->s_reclen);
	return 1;
}

static int do_fci_df(struct _sim *sim, const uint8_t *fci, size_t len)
{
	assert(len >= 13);
	printf(" o %u bytes under 0x%.4x\n",
		(fci[2] << 8) | fci[3], sim->s_pwd);
	printf(" o type of file (subclause 9.3): 0x%.2x\n", fci[6]);
	printf(" o %u bytes app specific\n", fci[12]);
	if ( fci[12] < 9 || fci[12] + 13 > len)
		return 0;
	printf(" o Contains %u DFs and %u EFs, %u CHV's\n",
		fci[14], fci[15], fci[16]);
	printf(" o CHV1: status=0x%.2x unblock=0x%.2x\n", fci[18], fci[19]);
	printf(" o CHV2: status=0x%.2x unblock=0x%.2x\n", fci[20], fci[21]);
	return 1;
}

static int parse_fci(struct _sim *sim, const uint8_t *fci, size_t len)
{
	switch ( sim->s_pwd & SIM_TYPE_MASK ) {
	case SIM_TYPE_MF:
	case SIM_TYPE_DF:
		return do_fci_df(sim, fci, len);
	case SIM_TYPE_EF:
	case SIM_TYPE_ROOT_EF:
		return do_fci_ef(sim, fci, len);
	default:
		return 0;
	}

	printf(" o Got %u byte FCI for 0x%.4x:\n", len, sim->s_pwd);
	hex_dump(fci, len, 16);
	return 1;
}

/* Recover from an undefined state after an error. */
static void undefined_state(sim_t sim)
{
	if ( do_select(sim, SIM_MF) )
		sim->s_pwd = SIM_MF;
}
 
const uint8_t *sim_read_binary(sim_t sim, size_t *rec_len)
{
	return NULL;
}

const uint8_t *sim_read_record(sim_t sim, uint8_t rec, size_t *rec_len)
{
	const uint8_t *ptr;
	uint8_t sw1, sw2;
	size_t data_len;

	switch ( sim->s_pwd & SIM_TYPE_MASK ) {
	case SIM_TYPE_EF:
	case SIM_TYPE_ROOT_EF:
		break;
	case SIM_TYPE_MF:
	case SIM_TYPE_DF:
	default:
		return NULL;
	}

	if ( !do_read_record(sim, rec, sim->s_reclen) )
		return 0;

	sw1 = xfr_rx_sw1(sim->s_xfr);
	switch( sw1 ) {
	case SIM_SW1_SHORT:
		sw2 = xfr_rx_sw2(sim->s_xfr);

		printf(" o rec_len disagreed %u != %u\n",
			sim->s_reclen, sw2);
		if ( !do_read_record(sim, rec, sw2) )
			return 0;

		sw1 = xfr_rx_sw1(sim->s_xfr);
		if ( sw1 != SIM_SW1_SUCCESS )
			return 0;
		break;
	case SIM_SW1_SUCCESS:
		break;
	default:
		return 0;
	}

	ptr = xfr_rx_data(sim->s_xfr, &data_len);
	if ( NULL == ptr )
		return NULL;

	if ( data_len != sim->s_reclen )
		printf(" o returned len disagreed %u != %u\n",
			sim->s_reclen, data_len);

	if ( rec_len )
		*rec_len = data_len;

#if 0
	printf(" o 0x%.4x record %u (%u bytes)\n",
		sim->s_pwd, rec, data_len);
	hex_dump(ptr, data_len, 16);
#endif
	return ptr;
}

int sim_select(sim_t sim, uint16_t id)
{
	uint8_t sw1, sw2;
	const uint8_t *fci;
	size_t fci_len;

	if ( !do_select(sim, id) )
		return 0;

	sw1 = xfr_rx_sw1(sim->s_xfr);
	switch( sw1 ) {
	case SIM_SW1_SHORT:
		sw2 = xfr_rx_sw2(sim->s_xfr);
		if ( !do_get_response(sim, sw2) )
			return 0;

		sw1 = xfr_rx_sw1(sim->s_xfr);
		if ( sw1 != SIM_SW1_SUCCESS )
			return 0;
		break;
	case SIM_SW1_SUCCESS:
		break;
	default:
		return 0;
	}

	sim->s_pwd = id;

	fci = xfr_rx_data(sim->s_xfr, &fci_len);
	if ( NULL == fci || !parse_fci(sim, fci, fci_len) ) {
		undefined_state(sim);
		return 0;
	}

	return 1;
}

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

	sim->s_pwd = SIM_MF;

	/* FIXME: Verify CLA by selecting MF */

	printf(" o Found SIM with %u byte ATR:\n", atr_len);
	hex_dump(atr, atr_len, 16);
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
