/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>
#include "sim-internal.h"
#include "bytesex.h"

static int do_select(struct _sim * s, uint16_t id)
{
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
	xfr_reset(s->s_xfr);
	xfr_tx_byte(s->s_xfr, SIM_CLA);
	xfr_tx_byte(s->s_xfr, SIM_INS_GET_RESPONSE);
	xfr_tx_byte(s->s_xfr, 0); /* p1 */
	xfr_tx_byte(s->s_xfr, 0); /* p2 */
	xfr_tx_byte(s->s_xfr, le);
	return chipcard_transact(s->s_cc, s->s_xfr);
}

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

	memset(&s->s_df_fci, 0, sizeof(s->s_df_fci));
	memset(&s->s_ef_fci, 0, sizeof(s->s_ef_fci));

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

	s->s_df_fci.f_free = sys_be16(s->s_df_fci.f_free);
	s->s_df_fci.f_id = sys_be16(s->s_df_fci.f_id);
	s->s_df = s->s_df_fci.f_id;
	s->s_ef = SIM_FILE_INVALID;
	printf("sim_apdu: DF 0x%.4x selected\n", s->s_df);

	printf(" df: %u bytes free\n", s->s_df_fci.f_free);

	if ( fci_len < s->s_df_fci.f_opt_len ) {
		printf("sim_apdu: DF/MF FCI optional data incomplete\n");
		return 0;
	}

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

//	s->s_df_gsm.g_fch;

	printf(" gsm: %u child DF's\n", s->s_df_gsm.g_num_df);
	printf(" gsm: %u child EF's\n", s->s_df_gsm.g_num_ef);
	printf(" gsm: %u locks n chains n shit\n", s->s_df_gsm.g_chv);
	chv_byte(s->s_df_gsm.g_chv1, "CHV1");
	chv_byte(s->s_df_gsm.g_chv1u, "CHV1 Unblock");
	chv_byte(s->s_df_gsm.g_chv2, "CHV2");
	chv_byte(s->s_df_gsm.g_chv2u, "CHV2 Unblock");

	return 1;
}

static void access_nibble(uint8_t n, const char *label)
{
	switch(n) {
	case 0:
		printf(" ef: access %s: ALW\n", label);
		return;
	case 1:
		printf(" ef: access %s: CHV1\n", label);
		return;
	case 2:
		printf(" ef: access %s: CHV2\n", label);
		return;
	case 4:
	case 0xe:
		printf(" ef: access %s: ADM\n", label);
		return;
	case 0xf:
		printf(" ef: access %s: NEV\n", label);
		return;
	default:
		printf(" ef: access %s: RFU %.1x\n", label, n);
		return;
	}
}

static int set_ef_fci(struct _sim *s)
{
	const uint8_t *fci;
	size_t fci_len;

	memset(&s->s_ef_fci, 0, sizeof(s->s_ef_fci));

	fci = xfr_rx_data(s->s_xfr, &fci_len);
	if ( NULL == fci )
		return 0;

	if ( fci_len < sizeof(s->s_ef_fci) ) {
		printf("sim_apdu: EF FCI incomplete\n");
		return 0;
	}
	memcpy(&s->s_ef_fci, fci, sizeof(s->s_ef_fci));
	fci += sizeof(s->s_ef_fci);
	fci_len -= sizeof(s->s_ef_fci);

	s->s_ef_fci.e_size = sys_be16(s->s_ef_fci.e_size);
	s->s_ef_fci.e_id = sys_be16(s->s_ef_fci.e_id);
	s->s_ef = s->s_ef_fci.e_id;
	printf("sim_apdu: EF 0x%.4x selected (parent DF = 0x%.4x)\n",
		s->s_ef, s->s_df);

	switch(s->s_ef_fci.e_struct) {
	case EF_TRANSPARENT:
		printf(" ef: transparent\n");
		break;
	case EF_LINEAR:
		printf(" ef: linear, rec_len = %u bytes\n",
			s->s_ef_fci.e_reclen);
		break;
	case EF_CYCLIC:
		printf(" ef: cyclic, rec_len = %u bytes\n",
			s->s_ef_fci.e_reclen);
		if ( 0x40 & s->s_ef_fci.e_increase )
			printf(" ef: INCREASE permitted\n");
		break;
	}
	printf(" ef: %u bytes\n", s->s_ef_fci.e_size);
	access_nibble(s->s_ef_fci.e_access[0] >> 4, "READ, SEEK");
	access_nibble(s->s_ef_fci.e_access[0] & 0xf, "UPDATE");
	access_nibble(s->s_ef_fci.e_access[1] >> 4, "INCREASE");
	access_nibble(s->s_ef_fci.e_access[2] >> 4, "INVALIDATE");
	access_nibble(s->s_ef_fci.e_access[2] & 0xf, "REHABILITATE");
	if ( s->s_ef_fci.e_status & 0x1 )
		printf(" ef: INVALIDATED\n");

	if ( fci_len < s->s_ef_fci.e_opt_len - EF_FCI_MIN_OPT_LEN) {
		printf("sim_apdu: EF FCI optional data incomplete\n");
		return 0;
	}

	if ( fci_len ) {
		printf("sim_apdu: %u bytes remaining data in FCI:\n", fci_len);
		hex_dump(fci, fci_len, 16);
	}

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
		set_df_fci(s, 1);
		break;
	case SIM_TYPE_DF:
		set_df_fci(s, 1);
		break;
	case SIM_TYPE_EF:
	case SIM_TYPE_ROOT_EF:
		set_ef_fci(s);
		break;
	default:
		printf("sim_apdu: unknown file type selected\n");
		break;
	}

	return 1;
}
