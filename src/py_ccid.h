/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/
#ifndef _PY_CCID_H
#define _PY_CCID_H

/* chipcard is the only dodgy part, this type can only be created via
 * a call to cci.get_slot()
 */
struct cp_chipcard {
	PyObject_HEAD;
	PyObject *owner;
	chipcard_t slot;
};

struct cp_cci {
	PyObject_HEAD;
	cci_t dev;
};

struct cp_xfr {
	PyObject_HEAD;
	xfr_t xfr;
};

#endif /* _PY_CCID_H */
