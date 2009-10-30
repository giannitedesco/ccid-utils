/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/
#ifndef _EMV_H
#define _EMV_H

#define EMV_RID_LEN	5
#define EMV_AID_LEN	11

typedef struct _emv *emv_t;
typedef struct _emv_app *emv_app_t;
typedef uint8_t emv_rid_t[EMV_RID_LEN];

/* Setup/teardown */
_public emv_t emv_init(chipcard_t cc);
_public void emv_fini(emv_t e);

/* Application selection */
_public emv_app_t emv_current_app(emv_t e);
_public int emv_appsel_pse(emv_t e);
_public emv_app_t emv_appsel_pse_first(emv_t e);
_public emv_app_t emv_appsel_pse_next(emv_t e, emv_app_t app);
_public int emv_app_select_pse(emv_t e, emv_app_t app);
_public int emv_app_select_aid(emv_t e, const uint8_t *aid, size_t len);
_public int emv_app_select_aid_next(emv_t e, const uint8_t *aid, size_t len);

/* EMV applications */
_public void emv_app_delete(emv_app_t a);
_public void emv_app_rid(emv_app_t a, emv_rid_t ret);
_public void emv_app_aid(emv_app_t a, uint8_t *ret, size_t *len);
_public const char *emv_app_label(emv_app_t a);
_public const char *emv_app_pname(emv_app_t a);
_public uint8_t emv_app_prio(emv_app_t a);
_public int emv_app_confirm(emv_app_t a);

/* Application initiation */
_public int emv_app_init(emv_t e);

/* Application data */
_public int emv_read_app_data(emv_t e);

#endif /* _EMV_H */
