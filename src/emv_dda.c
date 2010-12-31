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

struct dda_req {
	unsigned int ca_pk_idx;
	const uint8_t *pk_cert;
	size_t pk_cert_len;
	const uint8_t *pk_r;
	size_t pk_r_len;
	const uint8_t *pk_exp;
	size_t pk_exp_len;
	const uint8_t *icc_cert;
	size_t icc_cert_len;
	const uint8_t *icc_r;
	size_t icc_r_len;
	size_t icc_mod_len;
	const uint8_t *icc_exp;
	size_t icc_exp_len;
	const uint8_t *ddol;
	size_t ddol_len;
	const uint8_t *ssa_data;
	size_t ssa_data_len;
	uint8_t pan[10];
};

static int get_required_data(struct _emv *e, struct dda_req *req)
{
	const struct _emv_data *d;
	int i;

	d = _emv_retrieve_data(e, EMV_TAG_CA_PK_INDEX);
	i = emv_data_int(d);
	if ( i < 0 )
		return 0;

	req->ca_pk_idx = (unsigned int)i;

	d = _emv_retrieve_data(e, EMV_TAG_ISS_PK_CERT);
	if ( NULL == d )
		return 0;
	req->pk_cert = d->d_data;
	req->pk_cert_len = d->d_len;
	if ( NULL == req->pk_cert )
		return 0;

	d = _emv_retrieve_data(e, EMV_TAG_ISS_PK_R);
	if ( NULL == d )
		return 0;
	req->pk_r = d->d_data;
	req->pk_r_len = d->d_len;
	if ( NULL == req->pk_r )
		return 0;

	d = _emv_retrieve_data(e, EMV_TAG_ISS_PK_EXP);
	if ( NULL == d )
		return 0;
	req->pk_exp = d->d_data;
	req->pk_exp_len = d->d_len;
	if ( NULL == req->pk_exp)
		return 0;

	d = _emv_retrieve_data(e, EMV_TAG_ICC_PK_CERT);
	if ( NULL == d )
		return 0;
	req->icc_cert = d->d_data;
	req->icc_cert_len = d->d_len;
	if ( NULL == req->icc_cert )
		return 0;

	d = _emv_retrieve_data(e, EMV_TAG_ICC_PK_R);
	if ( NULL == d )
		return 0;
	req->icc_r = d->d_data;
	req->icc_r_len = d->d_len;
	if ( NULL == req->icc_r )
		return 0;

	d = _emv_retrieve_data(e, EMV_TAG_ICC_PK_EXP);
	if ( NULL == d )
		return 0;
	req->icc_exp = d->d_data;
	req->icc_exp_len = d->d_len;
	if ( NULL == req->icc_exp)
		return 0;

	d = _emv_retrieve_data(e, EMV_TAG_DDOL);
	if ( NULL == d )
		return 0;
	req->ddol = d->d_data;
	req->ddol_len = d->d_len;
	if ( NULL == req->ddol )
		return 0;

	d = _emv_retrieve_data(e, EMV_TAG_PAN);
	if ( NULL == d )
		return 0;
	memset(req->pan, 0xff, sizeof(req->pan));
	if ( d->d_len > sizeof(req->pan) )
		return 0;
	memcpy(req->pan, d->d_data, d->d_len);

	return 1;
}

static RSA *get_ca_key(unsigned int idx, emv_mod_cb_t mod,
			emv_exp_cb_t exp, size_t *key_len, void *priv)
{
	const uint8_t *modulus, *exponent;
	size_t mod_len, exp_len;
	RSA *key;

	modulus = (*mod)(priv, idx, &mod_len);
	if ( NULL == modulus )
		return NULL;

	exponent = (*exp)(priv, idx, &exp_len);
	if ( NULL == exponent )
		return NULL;
	
	key = RSA_new();
	if ( NULL == key )
		return NULL;

	key->n = BN_bin2bn(modulus, mod_len, NULL);
	if ( NULL == key->n )
		goto err_free_key;

	key->e = BN_bin2bn(exponent, exp_len, NULL);
	if ( NULL == key->e )
		goto err_free_mod;

	*key_len = mod_len;

	return key;
err_free_mod:
	BN_free(key->n);
err_free_key:
	RSA_free(key);
	return NULL;
}

static int recover(uint8_t *ptr, size_t len, RSA *key)
{
	uint8_t *tmp;
	int ret;

	tmp = malloc(len);
	if ( NULL == tmp )
		return 0;

	ret = RSA_public_encrypt(len, ptr, tmp, key, RSA_NO_PADDING);
	if ( ret < 0 || (unsigned)ret != len ) {
		free(tmp);
		return 0;
	}

	memcpy(ptr, tmp, len);
	free(tmp);

	return 1;
}

static int check_pk_cert(struct _emv *e, struct dda_req *req)
{
	uint8_t *msg, *tmp;
	size_t msg_len;
	int ret;

	if ( req->pk_cert[0] != 0x6a ) {
		printf("emv-dda: bad certificate header\n");
		return 0;
	}

	if ( req->pk_cert[1] != 0x02 ) {
		printf("emv-dda: bad certificate format\n");
		return 0;
	}

	if ( req->pk_cert[11] != 0x01 ) {
		printf("emv-dda: unexpected hash algorithm\n");
		return 0;
	}

	msg_len = req->pk_cert_len - (SHA_DIGEST_LENGTH + 2) +
			req->pk_r_len + req->pk_exp_len;
	tmp = msg = malloc(msg_len);
	if ( NULL == msg ) {
		_emv_sys_error(e);
		return 0;
	}

	memcpy(tmp, req->pk_cert + 1, req->pk_cert_len -
		(SHA_DIGEST_LENGTH + 2));
	tmp += req->pk_cert_len - (SHA_DIGEST_LENGTH + 2);
	memcpy(tmp, req->pk_r, req->pk_r_len);
	tmp += req->pk_r_len;
	memcpy(tmp, req->pk_exp, req->pk_exp_len);

	//printf("Encoded message of %u bytes:\n", msg_len);
	//hex_dump(msg, msg_len, 16);

	ret = _emsa_pss_decode(msg, msg_len, req->pk_cert, req->pk_cert_len);
	if ( !ret )
		_emv_error(e, EMV_ERR_CERTIFICATE);
	free(msg);

	return ret;
}

static RSA *make_issuer_pk(struct _emv *e, struct dda_req *req)
{
	uint8_t *tmp;
	const uint8_t *kb;
	size_t kb_len;
	RSA *key;

	tmp = malloc(req->pk_cert_len);
	if ( NULL == tmp ) {
		_emv_sys_error(e);
		return NULL;
	}

	kb = req->pk_cert + 15;
	kb_len = req->pk_cert_len - (15 + SHA_DIGEST_LENGTH + 1);

	memcpy(tmp, kb, kb_len);
	memcpy(tmp + kb_len, req->pk_r, req->pk_r_len);
	//printf("Retrieved issuer public key:\n");
	//hex_dump(kb, req->pk_cert_len, 16);
	key = RSA_new();
	if ( NULL == key ) {
		_emv_sys_error(e);
		free(tmp);
		return NULL;
	}

	key->n = BN_bin2bn(tmp, req->pk_cert_len, NULL);
	key->e = BN_bin2bn(req->pk_exp, req->pk_exp_len, NULL);
	free(tmp);
	if ( NULL == key->n || NULL == key->e ) {
		_emv_sys_error(e);
		RSA_free(key);
		return NULL;
	}

	return key;
}

static RSA *get_issuer_pk(struct _emv *e, struct dda_req *req,
				RSA *ca_key, size_t key_len)
{

	if ( req->pk_cert_len != key_len ) {
		_emv_error(e, EMV_ERR_KEY_SIZE_MISMATCH);
		return NULL;
	}

	if ( !recover((uint8_t *)req->pk_cert, key_len, ca_key) ) {
		_emv_error(e, EMV_ERR_RSA_RECOVERY);
		return NULL;
	}

	//printf("recovered issuer pubkey cert:\n");
	//hex_dump(req->pk_cert, key_len, 16);

	if ( !check_pk_cert(e, req) )
		return NULL;

	return make_issuer_pk(e, req);
}

static RSA *make_icc_pk(struct _emv *e, struct dda_req *req)
{
	uint8_t *tmp;
	const uint8_t *kb;
	size_t kb_len;
	RSA *key;

	tmp = malloc(req->icc_cert_len);
	if ( NULL == tmp ) {
		_emv_sys_error(e);
		return NULL;
	}

	kb = req->icc_cert + 21;
	kb_len = req->icc_cert_len - (21 + SHA_DIGEST_LENGTH + 1);
	req->icc_mod_len = kb_len + req->icc_r_len;

	memcpy(tmp, kb, kb_len);
	memcpy(tmp + kb_len, req->icc_r, req->icc_r_len);
	//printf("Retrieved ICC public key:\n");
	//hex_dump(tmp, req->icc_mod_len, 16);
	key = RSA_new();
	if ( NULL == key ) {
		_emv_sys_error(e);
		free(tmp);
		return NULL;
	}

	key->n = BN_bin2bn(tmp, req->icc_mod_len, NULL);
	key->e = BN_bin2bn(req->icc_exp, req->icc_exp_len, NULL);
	free(tmp);
	if ( NULL == key->n || NULL == key->e ) {
		_emv_sys_error(e);
		RSA_free(key);
		return NULL;
	}

	return key;
}

static int check_icc_cert(struct _emv *e, struct dda_req *req)
{
	struct _emv_data **rec;
	unsigned int num_rec, i;
	size_t msg_len, data_len;
	uint8_t *msg, *tmp;
	int ret;

	if ( req->icc_cert[0] != 0x6a ) {
		printf("emv-dda: bad certificate header\n");
		return 0;
	}

	if ( req->icc_cert[1] != 0x04 ) {
		printf("emv-dda: bad certificate format\n");
		return 0;
	}

	if ( req->icc_cert[17] != 0x01 ) {
		printf("emv-dda: unexpected hash algorithm\n");
		return 0;
	}

	rec = e->e_db.db_sda;
	num_rec = e->e_db.db_numsda;

	for(data_len = sizeof(e->e_aip), i = 0; i < num_rec; i++)
		data_len += rec[i]->d_len;

	msg_len = req->icc_cert_len - (SHA_DIGEST_LENGTH + 2) +
			req->icc_r_len + req->icc_exp_len + data_len;
	tmp = msg = malloc(msg_len);
	if ( NULL == msg ) {
		_emv_sys_error(e);
		return 0;
	}

	memcpy(tmp, req->icc_cert + 1, req->icc_cert_len -
		(SHA_DIGEST_LENGTH + 2));
	tmp += req->icc_cert_len - (SHA_DIGEST_LENGTH + 2);
	memcpy(tmp, req->icc_r, req->icc_r_len);
	tmp += req->icc_r_len;
	memcpy(tmp, req->icc_exp, req->icc_exp_len);
	tmp += req->icc_exp_len;

	for(i = 0; i < num_rec; i++) {
		memcpy(tmp, rec[i]->d_data, rec[i]->d_len);
		tmp += rec[i]->d_len;
	}
	memcpy(tmp, e->e_aip, sizeof(e->e_aip));

	//printf("Encoded message of %u bytes:\n", msg_len);
	//hex_dump(msg, msg_len, 16);

	ret = _emsa_pss_decode(msg, msg_len, req->icc_cert, req->icc_cert_len);
	if ( !ret )
		_emv_error(e, EMV_ERR_CERTIFICATE);
	free(msg);

	if ( ret && memcmp(req->icc_cert + 2, req->pan, sizeof(req->pan)) ) {
		printf("emv-dda: ICC certificate PAN mismatch\n");
		return 0;
	}

	e->e_sda_ok = 1;

	return ret;
}

static RSA *get_icc_pk(struct _emv *e, struct dda_req *req,
				RSA *iss_key, size_t key_len)
{
	if ( req->icc_cert_len != key_len ) {
		_emv_error(e, EMV_ERR_KEY_SIZE_MISMATCH);
		return NULL;
	}

	if ( !recover((uint8_t *)req->icc_cert, key_len, iss_key) ) {
		_emv_error(e, EMV_ERR_RSA_RECOVERY);
		return NULL;
	}

	//printf("recovered ICC pubkey cert:\n");
	//hex_dump(req->icc_cert, key_len, 16);

	if ( !check_icc_cert(e, req) )
		return NULL;

	return make_icc_pk(e, req);
}

static int dol_cb(uint16_t tag, uint8_t *ptr, size_t len, void *priv)
{
	//printf("DDOL tag: 0x%x\n", tag);
	switch(tag) {
	case EMV_TAG_UNPREDICTABLE_NUMBER:
#if 0
		unsigned int i;
		for(i = 0; i < len; i++)
			i = rand() & 0xff;
		return 1;
#else
		memset(ptr, 0, len);
#endif
		return 1;
	default:
		return 0;
		break;
	}
}

static int verify_dynamic_sig(emv_t e, size_t icc_pk_len,
				const uint8_t *ddol, size_t ddol_len)
{
	uint8_t hbuf[icc_pk_len + ddol_len];
	uint8_t md[SHA_DIGEST_LENGTH];
	uint8_t da[icc_pk_len];
	uint8_t *dol;
	size_t dol_len;
	const uint8_t *sig;
	size_t sig_len;
	int ret = 0;

	dol = _emv_construct_dol(dol_cb, ddol, ddol_len, &dol_len, NULL);
	if ( NULL == dol )
		return 0;

	//printf("Constructed DDOL:\n");
	//hex_dump(dol, dol_len, 16);

	if ( !_emv_int_authenticate(e, dol, dol_len) )
		goto out;

	sig = xfr_rx_data(e->e_xfr, &sig_len);
	if ( NULL == sig )
		goto out;
	
	if ( sig_len != icc_pk_len + 2 || icc_pk_len < 24 ||
			sig[0] != 0x80 || sig[1] != icc_pk_len ) {
		printf("Signed dynamic application data is corrupt\n");
		_emv_error(e, EMV_ERR_BER_DECODE);
		goto out;
	}

	memcpy(da, sig + 2, icc_pk_len);
	if ( !recover(da, icc_pk_len, e->e_icc_pk) ) {
		_emv_error(e, EMV_ERR_CERTIFICATE);
		goto out;
	}
	
	if ( da[0] != 0x6a || da[1] != 0x05 || da[2] != 0x01) {
		printf("Dynamic application data is corrupt\n");
		_emv_error(e, EMV_ERR_CERTIFICATE);
		goto out;
	}

	//printf("Signed authentication data:\n");
	//hex_dump(da, icc_pk_len, 16);

	memcpy(hbuf, da + 1, icc_pk_len - 22);
	memcpy(hbuf + (icc_pk_len - 22), dol, dol_len);
	//printf("Data covered by hash:\n");
	//hex_dump(hbuf, (icc_pk_len - 22) + dol_len, 16);

	SHA1(hbuf, (icc_pk_len - 22) + dol_len, md);

	if ( memcmp(md, (da + icc_pk_len) - (SHA_DIGEST_LENGTH + 1),
			SHA_DIGEST_LENGTH) ) {
		goto out;
	}
	ret = 1;
out:
	free(dol);
	return ret;
}

int emv_authenticate_dynamic(emv_t e, emv_mod_cb_t mod, emv_exp_cb_t exp,
					void *priv)
{
	struct dda_req req;
	size_t ca_key_len;
	RSA *ca_key;

	if ( e->e_dda_ok )
		return 1;

	if ( !(e->e_aip[0] & EMV_AIP_DDA) ) {
		_emv_error(e, EMV_ERR_FUNC_NOT_SUPPORTED);
		return 0;
	}

	if ( !get_required_data(e, &req) ) {
		_emv_error(e, EMV_ERR_DATA_ELEMENT_NOT_FOUND);
		return 0;
	}

	ca_key = get_ca_key(req.ca_pk_idx, mod, exp, &ca_key_len, priv);
	if ( NULL == ca_key ) {
		_emv_error(e, EMV_ERR_KEY_NOT_FOUND);
		return 0;
	}

	e->e_ca_pk = ca_key;

	e->e_iss_pk = get_issuer_pk(e, &req, ca_key, ca_key_len);
	if ( NULL == e->e_iss_pk )
		return 0;

	e->e_icc_pk = get_icc_pk(e, &req, e->e_iss_pk, ca_key_len);
	if ( NULL == e->e_icc_pk )
		return 0;

	if ( !verify_dynamic_sig(e, req.icc_mod_len, req.ddol, req.ddol_len) )
		return 0;

	e->e_dda_ok = 1;
	return 1;
}

int emv_dda_ok(emv_t e)
{
	return !!e->e_dda_ok;
}
