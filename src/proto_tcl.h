/*
 * This file is part of ccid-utils
 * Copyright (c) 2011 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/
#ifndef _PROTO_TCL_H
#define _PROTO_TCL_H

struct tcl_handle {
	/* derived from ats */
	unsigned int fsc;	/* max frame size accepted by card */
	unsigned int fsd;	/* max frame size accepted by reader */
	unsigned int fwt;	/* frame waiting time (in usec)*/
	unsigned char ta;	/* divisor information */
	unsigned char sfgt;	/* start-up frame guard time (in usec) */

	/* otherwise determined */
	unsigned int cid;	/* Card ID */
	unsigned int nad;	/* Node Address */

	unsigned int flags;
	unsigned int state;	/* protocol state */

	unsigned int toggle;	/* send toggle with next frame */
};

_private int _tcl_get_ats(struct _cci *cci, struct rfid_tag *tag,
			  struct tcl_handle *th);
_private int _tcl_transceive(struct _cci *cci, struct rfid_tag *tag,
			struct tcl_handle *th,
			const unsigned char *tx_data, unsigned int tx_len,
			unsigned char *rx_data, unsigned int *rx_len,
			unsigned int timeout);

#endif /* PROTO_TCL_H */
