#include <ccid.h>
#include <unistd.h>

#include "ccid-internal.h"
#include "rfid-internal.h"
#include "iso14443a.h"

void _rfid_dtor(struct _cci *cci)
{
	struct _rfid *rfid = cci->cc_priv;
	if ( rfid->rf_l1->dtor )
		rfid->rf_l1->dtor(rfid->rf_ccid, rfid->rf_l1p);
	free(rfid);
	cci->cc_priv = NULL;
}

int _rfid_select(struct _cci *cci)
{
	struct rfid_tag tag;

	if ( !_iso14443a_anticol(cci, 0, &tag) )
		return 0;

	printf("Found ISO-14443-A tag: cascade level %d\n",
		tag.level);
	hex_dump(tag.uid, tag.uid_len, 16);

	if ( tag.tcl_capable )
		return _tcl_get_ats(cci, &tag);

	printf("Unsupported tag type\n");
	return 0;
}
