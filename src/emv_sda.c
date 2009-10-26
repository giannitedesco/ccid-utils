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

static int check_iss_cert(struct _sda *s)
{
	size_t key_len = s->iss_cert_len;
	char md[SHA_DIGEST_LENGTH];
	SHA_CTX sha;

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

	if ( s->iss_cert[key_len - 1] != 0xbc ) {
		printf("error: certificate trailer b0rk\n");
		return 0;
	}

	printf("Decoded issuer certificate:\n");
	hex_dump(s->iss_cert, key_len, 16);

	/* TODO: EMSA-PSS decoding */
	SHA1_Init(&sha);
	SHA_Update(&sha, s->iss_cert + 1,
			key_len - (SHA_DIGEST_LENGTH + 2));
	SHA_Update(&sha, s->iss_pubkey_r, s->iss_pubkey_r_len);
	SHA_Update(&sha, s->iss_exp, s->iss_exp_len);
	SHA_Final(md, &sha);

	printf("Hash computed:\n");
	hex_dump(md, SHA_DIGEST_LENGTH, 20);

	printf("Certificate hash:\n");
	hex_dump(s->iss_cert + (key_len - 21), 20, 20);

	return 1;
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

int _sda_verify_ssa_data(struct _sda *s)
{
	const uint8_t *tmp;
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

	printf("Recovered SSA data:\n");
	hex_dump(tmp, ret, 16);

	memcpy(s->ssa_data, tmp, s->ssa_data_len);

	free(tmp);
	return 1;
}

