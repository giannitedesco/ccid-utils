/*
 * This file is part of cci-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
 *
 * Chip card transfer buffer management.
*/

#include <ccid.h>

#include <stdio.h>

#include "ccid-internal.h"

#define MIN_RESP_LEN 2U

struct _xfr *_xfr_do_alloc(size_t txbuf, size_t rxbuf)
{
	uint8_t *ptr;
	struct _xfr *xfr;
	size_t tot_len;

	tot_len = sizeof(*xfr) + txbuf + rxbuf + sizeof(*xfr->x_txhdr) * 2;
	ptr = calloc(1, tot_len);
	if ( NULL == ptr )
		return NULL;
	
	xfr = (struct _xfr *)ptr;
	ptr += sizeof(*xfr);

	xfr->x_txmax = txbuf;
	xfr->x_rxmax = txbuf;

	xfr->x_txhdr = (struct ccid_msg *)ptr;
	ptr += sizeof(*xfr->x_txhdr);

	xfr->x_txbuf = ptr;
	ptr += txbuf;

	xfr->x_rxhdr = (struct ccid_msg *)ptr;
	ptr += sizeof(*xfr->x_rxhdr);

	xfr->x_rxbuf = ptr;

	return xfr;
}

xfr_t xfr_alloc(size_t txbuf, size_t rxbuf)
{
	return _xfr_do_alloc(txbuf, rxbuf);
}

void xfr_reset(xfr_t xfr)
{
	xfr->x_txlen = xfr->x_rxlen = 0;
}

int xfr_tx_byte(xfr_t xfr, uint8_t byte)
{
	if ( xfr->x_txlen >= xfr->x_txmax )
		return 0;

	xfr->x_txbuf[xfr->x_txlen] = byte;
	xfr->x_txlen++;

	return 1;
}

int xfr_tx_buf(xfr_t xfr, const uint8_t *ptr, size_t len)
{
	if ( xfr->x_txlen + len > xfr->x_txmax )
		return 0;

	memcpy(xfr->x_txbuf + xfr->x_txlen, ptr, len);
	xfr->x_txlen += len;

	return 1;
}

uint8_t xfr_rx_sw1(xfr_t xfr)
{
	uint8_t *end;
	assert(xfr->x_rxlen >= MIN_RESP_LEN);
	end = xfr->x_rxbuf + xfr->x_rxlen;
	return end[-2];
}

uint8_t xfr_rx_sw2(xfr_t xfr)
{
	uint8_t *end;
	assert(xfr->x_rxlen >= MIN_RESP_LEN);
	end = xfr->x_rxbuf + xfr->x_rxlen;
	return end[-1];
}

const uint8_t *xfr_rx_data(xfr_t xfr, size_t *len)
{
	if ( xfr->x_rxlen < MIN_RESP_LEN )
		return NULL;
	*len = xfr->x_rxlen - MIN_RESP_LEN;
	return xfr->x_rxbuf;
}

void _xfr_do_free(struct _xfr *xfr)
{
	free(xfr);
}

void xfr_free(xfr_t xfr)
{
	_xfr_do_free(xfr);
}
