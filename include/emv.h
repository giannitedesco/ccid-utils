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

#define EMV_TAG_MAGSTRIP_TRACK2		0x0057
#define EMV_TAG_PAN			0x005a
#define EMV_TAG_RECORD			0x0070
#define EMV_TAG_CDOL1			0x008c
#define EMV_TAG_CDOL2			0x008d
#define EMV_TAG_CVM_LIST		0x008e
#define EMV_TAG_CA_PK_INDEX		0x008f
#define EMV_TAG_ISS_PK_CERT		0x0090
#define EMV_TAG_ISS_PK_R		0x0092
#define EMV_TAG_SSA_DATA		0x0093
#define EMV_TAG_CARDHOLDER_NAME		0x5f20
#define EMV_TAG_DATE_EXP		0x5f24
#define EMV_TAG_DATE_EFF		0x5f25
#define EMV_TAG_ISSUER_COUNTRY		0x5f28
#define EMV_TAG_SERVICE_CODE		0x5f30
#define EMV_TAG_PAN_SEQ			0x5f34
#define EMV_TAG_USAGE_CONTROL		0x9f07
#define EMV_TAG_APP_VER			0x9f08
#define EMV_TAG_IAC_DEFAULT		0x9f0d
#define EMV_TAG_IAC_DENY		0x9f0e
#define EMV_TAG_IAC_ONLINE		0x9f0f
#define EMV_TAG_MAGSTRIP_TRACK1		0x9f1f
#define EMV_TAG_ISS_PK_EXP		0x9f32
#define EMV_TAG_SDA_TAG_LIST		0x9f4a

#endif /* _EMV_H */
