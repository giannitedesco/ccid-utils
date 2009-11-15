/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>
#include <sim.h>
#include "sim-internal.h"

static void chv_byte(uint8_t b, const char *label)
{
	printf(" gsm: %s: %u tries remaining\n", label, b & 0xf);
	printf(" gsm: %s: secret code %sinitialized\n", label,
		(b & 0x80) ? "" : "not ");
}

static int set_df_fci(struct _sim *s, const uint8_t *fci, size_t fci_len)
{
	memset(&s->s_df_fci, 0, sizeof(s->s_df_fci));
	memset(&s->s_ef_fci, 0, sizeof(s->s_ef_fci));

	if ( fci_len < sizeof(s->s_df_fci) ) {
		printf("sim_apdu: DF/MF FCI incomplete\n");
		return 0;
	}
	memcpy(&s->s_df_fci, fci, sizeof(s->s_df_fci));
	fci += sizeof(s->s_df_fci);
	fci_len -= sizeof(s->s_df_fci);

	s->s_df_fci.d_free = sys_be16(s->s_df_fci.d_free);
	s->s_df_fci.d_id = sys_be16(s->s_df_fci.d_id);
	printf("sim_apdu: DF 0x%.4x selected\n", s->s_df);

	printf(" df: %u bytes free\n", s->s_df_fci.d_free);

	if ( fci_len < s->s_df_fci.d_opt_len ) {
		printf("sim_apdu: DF/MF FCI optional data incomplete\n");
		return 0;
	}

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

static int set_ef_fci(struct _sim *s, const uint8_t *fci, size_t fci_len)
{
	if ( fci_len < sizeof(s->s_ef_fci) ) {
		printf("sim_apdu: EF FCI incomplete\n");
		return 0;
	}
	memcpy(&s->s_ef_fci, fci, sizeof(s->s_ef_fci));
	fci += sizeof(s->s_ef_fci);
	fci_len -= sizeof(s->s_ef_fci);

	s->s_ef_fci.e_size = sys_be16(s->s_ef_fci.e_size);
	s->s_ef_fci.e_id = sys_be16(s->s_ef_fci.e_id);
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

static int set_fci(struct _sim *s, uint16_t id)
{
	const uint8_t *fci;
	size_t fci_len;
	struct fci f;

	memset(&s->s_ef_fci, 0, sizeof(s->s_ef_fci));

	fci = xfr_rx_data(s->s_xfr, &fci_len);
	if ( NULL == fci )
		return 0;

	if ( fci_len < sizeof(f) )
		return 0;

	memcpy(&f, fci, sizeof(f));
	f.f_id = sys_be16(f.f_id);

	switch(f.f_id & SIM_TYPE_MASK) {
	case SIM_TYPE_DF:
	case SIM_MF:
		s->s_df = f.f_id;
		s->s_ef = SIM_FILE_INVALID;
		if ( !set_df_fci(s, fci, fci_len) )
			return 0;
	case SIM_TYPE_EF:
	case SIM_TYPE_ROOT_EF:
		s->s_ef = f.f_id;
		if ( !set_ef_fci(s, fci, fci_len) )
			return 0;
	default:
		printf("sim_apdu: unknown file type selected\n");
		return (id == f.f_id);
	}

	return (id == f.f_id);
}

static int _sim_select(struct _sim *s, uint16_t id)
{
	if ( !_apdu_select(s, id) )
		return 0;

	return set_fci(s, id);
}

static const uint8_t *_sim_read_binary(struct _sim *s, size_t *len)
{
	if ( !_apdu_read_binary(s, 0, s->s_ef_fci.e_size) )
		return 0;

	return xfr_rx_data(s->s_xfr, len);
}

static const uint8_t *_sim_read_record(struct _sim *s, uint8_t rec, size_t *len)
{
	if ( !_apdu_read_record(s, rec, s->s_ef_fci.e_reclen) )
		return 0;

	return xfr_rx_data(s->s_xfr, len);
}

static void read_iccid(struct _sim *s)
{
	const uint8_t *ptr;
	size_t len, i;

	_sim_select(s, SIM_EF_ICCID);
	ptr = _sim_read_binary(s, &len);

	printf("ICCID: ");
	for(i = 0; i < len && ptr[i] != 0xff; i++) {
		if ( i && !(i % 2) )
			printf("-");
		printf("%.1x", ptr[i] & 0xf);
		if ( (ptr[i] >> 4) != 0xf )
			printf("%.1x", ptr[i] >> 4);
	}
	printf("\n");
}

int sim_sms_save(sim_t s, const char *fn)
{
	const uint8_t *ptr;
	size_t num_rec, i, len;
	FILE *f;

	if ( !_sim_select(s, SIM_DF_TELECOM) )
		return 0;

	if ( !_sim_select(s, SIM_EF_SMS) )
		return 0;

	f = fopen(fn, "w");
	if ( NULL == f )
		return 0;

	printf("Saving SMS messages to '%s'", fn);
	fflush(stdout);
	num_rec = s->s_ef_fci.e_size / s->s_ef_fci.e_reclen;
	for(i = 1; i <= num_rec; i++) {
		ptr = _sim_read_record(s, i, &len);
		if ( NULL == ptr )
			continue;
		fwrite(ptr, len, 1, f);
		printf(".");
		fflush(stdout);
	}
	printf("\n");

	fclose(f);
	return 1;
}

int sim_sms_restore(sim_t s, const char *fn)
{
	FILE *f;
	uint8_t buf[176];
	unsigned int i;
	struct _sms sms;

	f = fopen(fn, "r");
	if ( NULL == f )
		return 0;

	printf("Reading SMS messages from '%s':\n", fn);
	for(i = 1; fread(buf, sizeof(buf), 1, f) == 1; i++) {
		_sms_decode(&sms, buf);
	}

	fclose(f);
	return 1;
}

sim_t sim_new(chipcard_t cc)
{
	struct _sim *s;
	const uint8_t *atr;
	size_t atr_len;

	s = calloc(1, sizeof(*s));
	if ( NULL == s )
		goto err;

	s->s_cc = cc;

	s->s_xfr = xfr_alloc(502, 502);
	if ( NULL == s->s_xfr )
		goto err_free;

	atr = chipcard_slot_on(s->s_cc, CHIPCARD_AUTO_VOLTAGE, &atr_len);
	if ( NULL == atr )
		goto err_free_xfr;

	printf(" o Found SIM with %u byte ATR:\n", atr_len);
	hex_dump(atr, atr_len, 16);

	if ( !_sim_select(s, SIM_MF) )
		goto err_free_xfr;

	read_iccid(s);

	return s;

err_free_xfr:
	xfr_free(s->s_xfr);
err_free:
	free(s);
err:
	return NULL;
}

void sim_free(sim_t s)
{
	if ( s ) {
		if ( s->s_xfr )
			xfr_free(s->s_xfr);
		if ( s->s_cc )
			chipcard_slot_off(s->s_cc);
	}
	free(s);
}
