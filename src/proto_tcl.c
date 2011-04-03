#include <ccid.h>
#include <unistd.h>

#include "ccid-internal.h"
#include "rfid.h"
#include "iso14443a.h"
#include "proto_tcl.h"

#define FSD	5 /* 64 bytes */
#define CID	0
#define TIMEOUT	(((uint64_t)1000000 * 65536 / ISO14443_FREQ_CARRIER))
int _tcl_get_ats(struct _cci *cci, struct rfid_tag *tag)
{
	uint8_t ats[64];
	uint8_t rats[2];
	size_t ats_len;

	rats[0] = 0xe0;
	rats[1] = (CID & 0xf) | ((FSD & 0xf) << 4);

	ats_len = sizeof(ats);
	if ( !_iso14443ab_transceive(cci, RFID_14443A_FRAME_REGULAR,
				rats, sizeof(rats),
				ats, &ats_len,
				TIMEOUT) )
		return 0;

	printf("tcl: Got %zu bytes ATS...\n", ats_len);
	hex_dump(ats, ats_len, 16);
	return 0;
}