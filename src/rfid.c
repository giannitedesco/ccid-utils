#include <ccid.h>
#include <unistd.h>

#include "ccid-internal.h"
#include "rfid.h"
#include "iso14443a.h"

int _rfid_select(struct _cci *cci)
{
	struct rfid_tag tag;

	if ( !_iso14443a_anticol(cci, 0, &tag) )
		return 0;

	printf("Found ISO-14443-A tag: cascade level %d\n",
		tag.level);
	hex_dump(tag.uid, tag.uid_len, 16);

	if ( tag.tcl_capable ) {
		printf("Sending RATS\n");
		return 1;
	}

	/* proprietary */
	return 0;
}
