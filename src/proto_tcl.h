/*
 * This file is part of ccid-utils
 * Copyright (c) 2011 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/
#ifndef _PROTO_TCL_H
#define _PROTO_TCL_H

struct tcl_handle;
_private int _tcl_get_ats(struct _cci *cci, struct rfid_tag *tag);
_private int _tcl_transceive(struct _cci *cci, struct rfid_tag *tag,
			struct tcl_handle *th,
			const unsigned char *tx_data, unsigned int tx_len,
			unsigned char *rx_data, unsigned int *rx_len,
			unsigned int timeout);

#endif /* PROTO_TCL_H */
