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

static int emsa_pss_decode(const uint8_t *msg, size_t msg_len,
				const uint8_t *em, size_t em_len)
{
	uint8_t md[SHA_DIGEST_LENGTH];
	size_t mdb_len;

	/* 2. mHash < Hash(m) */
	SHA1(msg, msg_len, md);

	/* 4. if the rightmost octet of em does not have hexadecimal
	 * value 0xBC, output “invalid” */
	if ( em[em_len - 1] != 0xbc ) {
		printf("emsa-pss: bad trailer\n");
		return 0;
	}
	
	mdb_len = em_len - sizeof(md) - 1;

	if ( memcmp(em + mdb_len, md, SHA_DIGEST_LENGTH) )
		return 0;

	return 1;
}

static int check_iss_cert(struct _sda *s)
{
	size_t key_len = s->iss_cert_len;
	uint8_t *msg, *tmp;
	size_t msg_len;

	if ( s->iss_cert[0] != 0x6a ) {
		printf("error: certificate header b0rk\n");
		return 0;
	}

	if ( s->iss_cert[1] != 0x02 ) {
		printf("error: certificate format b0rk\n");
		return 0;
	}

	if ( s->iss_cert[11] != 0x01 ) {
		printf("error: unexpected hash format in certificate\n");
		return 0;
	}

	//printf("Decoded issuer certificate:\n");
	//hex_dump(s->iss_cert, key_len, 16);

	msg_len = key_len - (SHA_DIGEST_LENGTH + 2) +
			s->iss_pubkey_r_len + s->iss_exp_len;
	tmp = msg = malloc(msg_len);
	if ( NULL == msg )
		return 0;

	memcpy(tmp, s->iss_cert + 1, key_len - (SHA_DIGEST_LENGTH + 2));
	tmp += key_len - (SHA_DIGEST_LENGTH + 2);
	memcpy(tmp, s->iss_pubkey_r, s->iss_pubkey_r_len);
	tmp += s->iss_pubkey_r_len;
	memcpy(tmp, s->iss_exp, s->iss_exp_len);

	//printf("Encoded message of %u bytes:\n", msg_len);
	//hex_dump(msg, msg_len, 16);

	return emsa_pss_decode(msg, msg_len, s->iss_cert, s->iss_cert_len);
}

int _sda_get_issuer_key(struct _sda *s, RSA *ca_key, size_t key_len)
{
	uint8_t *tmp, *kb;
	size_t kb_len;
	int ret;

	assert(key_len >= 16);

	if ( s->iss_cert_len != key_len ) {
		printf("error: issuer key/cert len mismatch\n");
		return 0;
	}

	tmp = malloc(key_len);
	assert(NULL != tmp);

	ret = RSA_public_encrypt(key_len, s->iss_cert, tmp, ca_key,
					RSA_NO_PADDING);
	if ( ret < 0 || (unsigned)ret != key_len ) {
		printf("error: rsa failed on issuer key certificate\n");
		free(tmp);
		return 0;
	}

	memcpy(s->iss_cert, tmp, key_len);
	kb = s->iss_cert + 15;
	kb_len = key_len - (15 + 21);

	if ( kb_len + s->iss_pubkey_r_len != key_len ) {
		printf("error: not enough bytes for issuer public key\n");
		free(tmp);
		return 0;
	}

	if ( !check_iss_cert(s) ) {
		free(tmp);
		return 0;
	}

	printf("Issuer certificate hash OK\n");

	/* stitch together key bytes from certificate and separate
	 * key remainder field... */
	memcpy(tmp, kb, kb_len);
	memcpy(tmp + kb_len, s->iss_pubkey_r, s->iss_pubkey_r_len);
	printf("Retrieved issuer public key:\n");
	hex_dump(tmp, key_len, 16);

	/* key remainder no longer required */
	free(s->iss_pubkey_r);
	s->iss_pubkey_r = NULL;
	s->iss_pubkey_r_len = 0;

	s->iss_pubkey = RSA_new();
	s->iss_pubkey->n = BN_bin2bn(tmp, key_len, NULL);
	s->iss_pubkey->e = BN_bin2bn(s->iss_exp,
					s->iss_exp_len, NULL);
	free(tmp);
	return 1;
}

static int check_ssa_data(struct _sda *s)
{
	uint8_t *msg, *cf;
	size_t msg_len, cf_len;

	if ( s->ssa_data[0] != 0x6a ) {
		printf("error: SSA data bad signature byte\n");
		return 0;
	}
	
	if ( s->ssa_data[1] != 0x03 ) {
		printf("error: SSA data bad format\n");
		return 0;
	}

	if ( s->ssa_data[2] != 0x01 ) {
		printf("error: unexpected hash format in SSA data\n");
		return 0;
	}

	cf = s->ssa_data + 1;
	cf_len = s->ssa_data_len - (SHA_DIGEST_LENGTH + 2);
	msg_len = cf_len + s->data_len;

//	printf("Signed SSA data:\n");
//	hex_dump(s->ssa_data, s->ssa_data_len, 16);

//	printf("Data for authentication:\n");
//	hex_dump(s->data, s->data_len, 16);

	msg = malloc(msg_len);
	memcpy(msg, cf, cf_len);
	memcpy(msg + cf_len, s->data, s->data_len);
//	printf("%u byte SSA message:\n", msg_len);
//	hex_dump(msg, msg_len, 16);

	if ( !emsa_pss_decode(msg, msg_len, s->ssa_data, s->ssa_data_len) ) {
		free(msg);
		return 0;
	}
	free(msg);
	return 1;
}

int _sda_verify_ssa_data(struct _sda *s)
{
	uint8_t *tmp;
	unsigned int ret;

	tmp = malloc(s->ssa_data_len);
	if ( NULL == tmp )
		return 0;

	ret = RSA_public_encrypt(s->ssa_data_len,
					s->ssa_data, tmp,
					s->iss_pubkey,
					RSA_NO_PADDING);
	if ( ret < 0 || (unsigned)ret != s->ssa_data_len ) {
		printf("error: rsa failed on SSA data\n");
		free(tmp);
		return 0;
	}

	memcpy(s->ssa_data, tmp, s->ssa_data_len);
	free(tmp);

	if ( s->ssa_data_len != s->iss_cert_len ) {
		printf("error: SSA data length check failed\n");
		return 0;
	}

	if ( !check_ssa_data(s) ) {
		printf("SSA data verification failed\n");
		return 0;
	}

	printf("SSA data verified\n");
	return 1;
}

