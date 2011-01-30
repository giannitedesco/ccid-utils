/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>
#include <sim.h>
#include "sim-internal.h"

#if DEBUG
#define dprintf print
#else
#define dprintf(...) do {} while(0)
#endif

static void chv_byte(uint8_t b, const char *label)
{
	dprintf(" gsm: %s: %u tries remaining\n", label, b & 0xf);
	dprintf(" gsm: %s: secret code %sinitialized\n", label,
		(b & 0x80) ? "" : "not ");
}

static int set_df_fci(struct _sim *s, const uint8_t *fci, size_t fci_len)
{
	memset(&s->s_df_fci, 0, sizeof(s->s_df_fci));
	memset(&s->s_ef_fci, 0, sizeof(s->s_ef_fci));

	if ( fci_len < sizeof(s->s_df_fci) ) {
		dprintf("sim_apdu: DF/MF FCI incomplete\n");
		return 0;
	}
	memcpy(&s->s_df_fci, fci, sizeof(s->s_df_fci));
	fci += sizeof(s->s_df_fci);
	fci_len -= sizeof(s->s_df_fci);

	s->s_df_fci.d_free = be16toh(s->s_df_fci.d_free);
	s->s_df_fci.d_id = be16toh(s->s_df_fci.d_id);
	dprintf("sim_apdu: DF 0x%.4x selected\n", s->s_df);

	dprintf(" df: %u bytes free\n", s->s_df_fci.d_free);

	if ( fci_len < s->s_df_fci.d_opt_len ) {
		dprintf("sim_apdu: DF/MF FCI optional data incomplete\n");
		return 0;
	}

	if ( fci_len < sizeof(struct df_gsm) ) {
		dprintf("sim_apdu: DF/MF FCI GSM data incomplete\n");
		return 0;
	}

	memcpy(&s->s_df_gsm, fci, sizeof(s->s_df_gsm));
	fci += sizeof(s->s_df_gsm);
	fci_len -= sizeof(s->s_df_gsm);

	if ( fci_len ) {
		dprintf("sim_apdu: %u bytes remaining data in FCI:\n", fci_len);
		hex_dump(fci, fci_len, 16);
	}

//	s->s_df_gsm.g_fch;

	dprintf(" gsm: %u child DF's\n", s->s_df_gsm.g_num_df);
	dprintf(" gsm: %u child EF's\n", s->s_df_gsm.g_num_ef);
	dprintf(" gsm: %u locks n chains n shit\n", s->s_df_gsm.g_chv);
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
		dprintf(" ef: access %s: ALW\n", label);
		return;
	case 1:
		dprintf(" ef: access %s: CHV1\n", label);
		return;
	case 2:
		dprintf(" ef: access %s: CHV2\n", label);
		return;
	case 4:
	case 0xe:
		dprintf(" ef: access %s: ADM\n", label);
		return;
	case 0xf:
		dprintf(" ef: access %s: NEV\n", label);
		return;
	default:
		dprintf(" ef: access %s: RFU %.1x\n", label, n);
		return;
	}
}

static int set_ef_fci(struct _sim *s, const uint8_t *fci, size_t fci_len)
{
	if ( fci_len < sizeof(s->s_ef_fci) ) {
		dprintf("sim_apdu: EF FCI incomplete\n");
		return 0;
	}
	memcpy(&s->s_ef_fci, fci, sizeof(s->s_ef_fci));
	fci += sizeof(s->s_ef_fci);
	fci_len -= sizeof(s->s_ef_fci);

	s->s_ef_fci.e_size = be16toh(s->s_ef_fci.e_size);
	s->s_ef_fci.e_id = be16toh(s->s_ef_fci.e_id);
	dprintf("sim_apdu: EF 0x%.4x selected (parent DF = 0x%.4x)\n",
		s->s_ef, s->s_df);

	switch(s->s_ef_fci.e_struct) {
	case EF_TRANSPARENT:
		dprintf(" ef: transparent\n");
		break;
	case EF_LINEAR:
		dprintf(" ef: linear, rec_len = %u bytes\n",
			s->s_ef_fci.e_reclen);
		break;
	case EF_CYCLIC:
		dprintf(" ef: cyclic, rec_len = %u bytes\n",
			s->s_ef_fci.e_reclen);
		if ( 0x40 & s->s_ef_fci.e_increase )
			dprintf(" ef: INCREASE permitted\n");
		break;
	}
	dprintf(" ef: %u bytes\n", s->s_ef_fci.e_size);
	access_nibble(s->s_ef_fci.e_access[0] >> 4, "READ, SEEK");
	access_nibble(s->s_ef_fci.e_access[0] & 0xf, "UPDATE");
	access_nibble(s->s_ef_fci.e_access[1] >> 4, "INCREASE");
	access_nibble(s->s_ef_fci.e_access[2] >> 4, "INVALIDATE");
	access_nibble(s->s_ef_fci.e_access[2] & 0xf, "REHABILITATE");
	if ( s->s_ef_fci.e_status & 0x1 )
		dprintf(" ef: INVALIDATED\n");

	if ( fci_len < s->s_ef_fci.e_opt_len - EF_FCI_MIN_OPT_LEN) {
		dprintf("sim_apdu: EF FCI optional data incomplete\n");
		return 0;
	}

	if ( fci_len ) {
		dprintf("sim_apdu: %u bytes remaining data in FCI:\n", fci_len);
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
	f.f_id = be16toh(f.f_id);

	switch(f.f_id & SIM_TYPE_MASK) {
	case SIM_TYPE_DF:
	case SIM_MF:
		s->s_df = f.f_id;
		s->s_ef = SIM_FILE_INVALID;
		if ( !set_df_fci(s, fci, fci_len) )
			return 0;
		break;
	case SIM_TYPE_EF:
	case SIM_TYPE_ROOT_EF:
		s->s_ef = f.f_id;
		if ( !set_ef_fci(s, fci, fci_len) )
			return 0;
		break;
	default:
		dprintf("sim_apdu: unknown file type selected\n");
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

static void read_imsi(struct _sim *s)
{
	const uint8_t *ptr;
	size_t len, i;

	_sim_select(s, SIM_DF_GSM);
	_sim_select(s, SIM_EF_IMSI);
	ptr = _sim_read_binary(s, &len);

	printf("IMSI: ");
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

sim_t sim_new(cci_t cc)
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

	atr = cci_power_on(s->s_cc, CHIPCARD_AUTO_VOLTAGE, &atr_len);
	if ( NULL == atr )
		goto err_free_xfr;

	printf(" o Found SIM with %u byte ATR:\n", atr_len);
	hex_dump(atr, atr_len, 16);

	if ( !_sim_select(s, SIM_MF) )
		goto err_free_xfr;

	read_iccid(s);
	read_imsi(s);

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
			cci_power_off(s->s_cc);
	}
	free(s);
}
