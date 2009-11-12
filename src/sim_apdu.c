/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>
#include "sim-internal.h"

static int do_select(struct _sim * s, uint16_t id)
{
	printf("sim_apdu: SELECT_FILE 0x%.4x\n", id);
	xfr_reset(s->s_xfr);
	xfr_tx_byte(s->s_xfr, SIM_CLA);
	xfr_tx_byte(s->s_xfr, SIM_INS_SELECT);
	xfr_tx_byte(s->s_xfr, 0); /* p1 */
	xfr_tx_byte(s->s_xfr, 0); /* p2 */
	xfr_tx_byte(s->s_xfr, 2); /* lc */
	xfr_tx_byte(s->s_xfr, (id >> 8));
	xfr_tx_byte(s->s_xfr, (id & 0xff));
	return chipcard_transact(s->s_cc, s->s_xfr);
}

static int do_get_response(struct _sim * s, uint8_t le)
{
	printf("sim_apdu: GET_RESPONSE %u bytes\n", le);
	xfr_reset(s->s_xfr);
	xfr_tx_byte(s->s_xfr, SIM_CLA);
	xfr_tx_byte(s->s_xfr, SIM_INS_GET_RESPONSE);
	xfr_tx_byte(s->s_xfr, 0); /* p1 */
	xfr_tx_byte(s->s_xfr, 0); /* p2 */
	xfr_tx_byte(s->s_xfr, le);
	return chipcard_transact(s->s_cc, s->s_xfr);
}

#if 0
static int do_fci_ef(struct _sim *s, const uint8_t *fci, size_t len)
{
	assert(len == 15);
	s->s_reclen = fci[14];
	printf("sim_apdu: Record len %u\n", s->s_reclen);
	return 1;
}

static int do_fci_df(struct _sim *s, const uint8_t *fci, size_t len)
{
	assert(len >= 13);
	printf("sim_apdu: %u bytes under 0x%.4x\n",
		(fci[2] << 8) | fci[3], s->s_pwd);
	printf("sim_apdu: type of file (subclause 9.3): 0x%.2x\n", fci[6]);
	printf("sim_apdu: %u bytes app specific\n", fci[12]);
	if ( fci[12] < 9 || fci[12] + 13 > len)
		return 0;
	printf("sim_apdu: Contains %u DFs and %u EFs, %u CHV's\n",
		fci[14], fci[15], fci[16]);
	printf("sim_apdu: CHV1: status=0x%.2x unblock=0x%.2x\n", fci[18], fci[19]);
	printf("sim_apdu: CHV2: status=0x%.2x unblock=0x%.2x\n", fci[20], fci[21]);
	return 1;
}

static int parse_fci(struct _sim *s, const uint8_t *fci, size_t len)
{
	switch ( s->s_pwd & SIM_TYPE_MASK ) {
	case SIM_TYPE_MF:
	case SIM_TYPE_DF:
		return do_fci_df(s, fci, len);
	case SIM_TYPE_EF:
	case SIM_TYPE_ROOT_EF:
		return do_fci_ef(s, fci, len);
	default:
		return 0;
	}

	printf("sim_apdu: Got %u byte FCI for 0x%.4x:\n", len, s->s_pwd);
	hex_dump(fci, len, 16);
	return 1;
}
#endif

static void chv_byte(uint8_t b, const char *label)
{
	printf(" gsm: %s: %u tries remaining\n", label, b & 0xf);
	printf(" gsm: %s: secret code %sinitialized\n", label,
		(b & 0x80) ? "" : "not ");
}

static int set_df_fci(struct _sim *s, int gsm)
{
	const uint8_t *fci;
	size_t fci_len;

	fci = xfr_rx_data(s->s_xfr, &fci_len);
	if ( NULL == fci )
		return 0;

	if ( fci_len < sizeof(s->s_df_fci) ) {
		printf("sim_apdu: DF/MF FCI incomplete\n");
		return 0;
	}
	memcpy(&s->s_df_fci, fci, sizeof(s->s_df_fci));
	fci += sizeof(s->s_df_fci);
	fci_len -= sizeof(s->s_df_fci);

	if ( fci_len < s->s_df_fci.f_opt_len ) {
		printf("sim_apdu: DF/MF FCI optional data incomplete\n");
		return 0;
	}

	printf(" df: %u bytes free\n", s->s_df_fci.f_free);

	if ( !gsm )
		return 1;

	if ( fci_len < sizeof(struct df_gsm) ) {
		printf("sim_apdu: DF/MF FCI GSM data incomplete\n");
		return 0;
	}

	memcpy(&s->s_df_gsm, fci, sizeof(s->s_df_gsm));
	fci += sizeof(s->s_df_gsm);
	fci_len -= sizeof(s->s_df_gsm);

	if ( fci_len ) {
		printf("sim_apdu: %u bytes remaining data in FCI:\n", fci_len);
		hex_dump(fci, fci_len, 16);
	}

	s->s_df_gsm.g_fch;
	printf(" gsm: %u child DF's\n", s->s_df_gsm.g_num_df);
	printf(" gsm: %u child EF's\n", s->s_df_gsm.g_num_ef);
	printf(" gsm: %u locks n chains n shit\n", s->s_df_gsm.g_chv);
	chv_byte(s->s_df_gsm.g_chv1, "CHV1");
	chv_byte(s->s_df_gsm.g_chv1u, "CHV1 Unblock");
	chv_byte(s->s_df_gsm.g_chv2, "CHV2");
	chv_byte(s->s_df_gsm.g_chv2u, "CHV2 Unblock");

	return 1;
}

static int set_ef_fci(struct _sim *s)
{
	return 1;
}

int _sim_select(struct _sim *s, uint16_t id)
{
	uint8_t sw1, sw2;

	if ( !do_select(s, id) )
		return 0;

	sw1 = xfr_rx_sw1(s->s_xfr);
	if ( sw1 != SIM_SW1_SHORT )
		return 0;

	sw2 = xfr_rx_sw2(s->s_xfr);
	if ( !do_get_response(s, sw2) )
		return 0;

	sw1 = xfr_rx_sw1(s->s_xfr);
	if ( sw1 != SIM_SW1_SUCCESS )
		return 0;

	switch(id & SIM_TYPE_MASK) {
	case SIM_MF:
		s->s_df = id;
		s->s_ef = SIM_FILE_INVALID;
		printf("sim_apdu: MF selected\n");
		set_df_fci(s, 1);
		break;
	case SIM_TYPE_DF:
		s->s_df = id;
		s->s_ef = SIM_FILE_INVALID;
		printf("sim_apdu: DF selected\n");
		set_df_fci(s, 1);
		break;
	case SIM_TYPE_EF:
		s->s_ef = id;
		printf("sim_apdu: EF selected (parent DF = 0x%.4x\n", s->s_df);
		set_ef_fci(s);
		break;
	}

	return 1;
}
