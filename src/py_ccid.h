/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/
#ifndef _PY_CCID_H
#define _PY_CCID_H

struct cp_devlist {
	PyObject_HEAD;
	ccidev_t *list;
	size_t nmemb;
};

struct cp_dev {
	PyObject_HEAD;
	ccidev_t dev;
	struct cp_devlist *owner;
	size_t idx;
};

/* chipcard is the only dodgy part, this type can only be created via
 * a call to cci.get_slot()
 */
struct cp_cci {
	PyObject_HEAD;
	PyObject *owner;
	cci_t slot;
	unsigned int index;
};

struct cp_ccid {
	PyObject_HEAD;
	ccid_t dev;
};

struct cp_xfr {
	PyObject_HEAD;
	xfr_t xfr;
};

#endif /* _PY_CCID_H */
