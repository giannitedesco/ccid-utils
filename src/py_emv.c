/*
 * This file is part of cci-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>
#include <emv.h>
#include <Python.h>
#include <structmember.h>
#include "py_ccid.h"

struct cp_emv {
	PyObject_HEAD;
	emv_t emv;
};

struct cp_app {
	PyObject_HEAD;
	emv_app_t app;
	struct cp_emv *owner;
};

struct cp_data {
	PyObject_HEAD;
	emv_data_t data;
	struct cp_emv *owner;
};

static PyObject *data_dict(struct cp_emv *, emv_data_t *, unsigned int);

static PyObject *bcd_convert(const uint8_t *ptr, size_t len)
{
	char buf[len * 2 + len / 2 + 1];
	size_t i;
	char *p;

	for(i = 0, p = buf; i < len; i++, p += 2) {
		if ( i && 0 == (i % 2) )
			sprintf(p, "-"), ++p;
		sprintf(p, "%.2x", ptr[i]);
	}

	return PyString_FromString(buf);
}

static PyObject *binary_convert(const uint8_t *ptr, size_t len)
{
	char buf[len * 3 + 1];
	size_t i;
	char *p;

	for(i = 0, p = buf; i < len; i++, p += 3)
		sprintf(p, "%.2x ", ptr[i]);
	p--;
	*p = '\0';

	return PyString_FromString(buf);
}

static PyObject *date_convert(const uint8_t *ptr, size_t len)
{
	char buf[11];
	snprintf(buf, sizeof(buf), "%.2u%.2x-%.2x-%.2x",
		(ptr[0] < 70) ? 20 : 19, ptr[0], ptr[1], ptr[2]);
	return PyString_FromString(buf);
}

static PyObject *cp_data_value(struct cp_data *self, PyObject *args)
{
	const uint8_t *ptr;
	size_t len;

	ptr = emv_data(self->data, &len);

	switch(emv_data_type(self->data)) {
	case EMV_DATA_BINARY:
		return binary_convert(ptr, len);
	case EMV_DATA_TEXT:
		return PyString_FromStringAndSize((const char *)ptr, len);
	case EMV_DATA_INT:
		return PyInt_FromLong(emv_data_int(self->data));
	case EMV_DATA_BCD:
		return bcd_convert(ptr, len);
	case EMV_DATA_DATE:
		return date_convert(ptr, len);
	default:
		Py_INCREF(Py_None);
		return Py_None;
	}
}

static PyObject *cp_data_children(struct cp_data *self, PyObject *args)
{
	emv_data_t *rec;
	unsigned int num_rec;

	rec = emv_data_children(self->data, &num_rec);
	return data_dict(self->owner, rec, num_rec);
}

static PyObject *cp_data_type(struct cp_data *self, PyObject *args)
{
	return Py_BuildValue("i", emv_data_type(self->data));
}

static PyObject *cp_data_tag(struct cp_data *self, PyObject *args)
{
	return Py_BuildValue("i", emv_data_tag(self->data));
}

static PyObject *cp_data_tag_label(struct cp_data *self, PyObject *args)
{
	const char *label;
	label = emv_data_tag_label(self->data);
	if ( label ) {
		return PyString_FromString(label);
	}else{
		Py_INCREF(Py_None);
		return Py_None;
	}
}

static void cp_data_dealloc(struct cp_data *self)
{
	Py_DECREF(self->owner);
	self->ob_type->tp_free((PyObject*)self);
}

static PyMethodDef cp_data_methods[] = {
	{"type",(PyCFunction)cp_data_type, METH_VARARGS,
		"data.type() - Data element type"},
	{"tag",(PyCFunction)cp_data_tag, METH_VARARGS,
		"data.type() - Data element BER tag"},
	{"tag_label",(PyCFunction)cp_data_tag_label, METH_VARARGS,
		"data.type() - Data element tag name if known, None if not"},
	{"children",(PyCFunction)cp_data_children, METH_VARARGS,
		"data.children() - Returns dictionary of child elements"},
	{"value",(PyCFunction)cp_data_value, METH_VARARGS,
		"data.value() - Retrieve value of data element"},
	{NULL,},
};

static PyTypeObject data_pytype = {
	PyObject_HEAD_INIT(NULL)
	.tp_name = "emv.data",
	.tp_basicsize = sizeof(struct cp_data),
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_new = PyType_GenericNew,
	.tp_dealloc = (destructor)cp_data_dealloc,
	.tp_methods = cp_data_methods,
	.tp_doc = "EMV data element",
};

static PyObject *data_dict(struct cp_emv *owner, emv_data_t *elem,
				unsigned int nmemb)
{
	PyObject *dict;
	unsigned int i;

	dict = PyDict_New();
	for(i = 0; i < nmemb; i++) {
		PyObject *key;
		struct cp_data *val;

		val = (struct cp_data *)_PyObject_New(&data_pytype);
		val->data = elem[i];
		val->owner = owner;
		Py_INCREF(owner);

		key = PyInt_FromLong(emv_data_tag(elem[i]));

		PyDict_SetItem(dict, key, (PyObject *)val);
	}

	return dict;
}

static void cp_app_dealloc(struct cp_app *self)
{
	Py_DECREF(self->owner);
	emv_app_delete(self->app);
	self->ob_type->tp_free((PyObject*)self);
}

static PyObject *cp_app_rid(struct cp_app *self, PyObject *args)
{
	emv_rid_t ret;
	emv_app_rid(self->app, ret);
	return Py_BuildValue("s#", ret, EMV_RID_LEN);
}

static PyObject *cp_app_aid(struct cp_app *self, PyObject *args)
{
	uint8_t ret[EMV_AID_LEN];
	size_t len;
	emv_app_aid(self->app, ret, &len);
	return Py_BuildValue("s#", ret, len);
}

static PyObject *cp_app_label(struct cp_app *self, PyObject *args)
{
	return Py_BuildValue("s", emv_app_label(self->app));
}

static PyObject *cp_app_pname(struct cp_app *self, PyObject *args)
{
	return Py_BuildValue("s", emv_app_pname(self->app));
}

static PyObject *cp_app_prio(struct cp_app *self, PyObject *args)
{
	return Py_BuildValue("i", emv_app_prio(self->app));
}

static PyObject *cp_app_confirm(struct cp_app *self, PyObject *args)
{
	return Py_BuildValue("i", emv_app_confirm(self->app));
}

static PyMethodDef cp_app_methods[] = {
	{"rid",(PyCFunction)cp_app_rid, METH_VARARGS,
		"app.rid() - Registered application ID"},
	{"aid",(PyCFunction)cp_app_aid, METH_VARARGS,
		"app.aid() - Application ID"},
	{"label",(PyCFunction)cp_app_label, METH_VARARGS,
		"app.name() - Label"},
	{"pname",(PyCFunction)cp_app_pname, METH_VARARGS,
		"app.pname() - Preferred name"},
	{"prio",(PyCFunction)cp_app_prio, METH_VARARGS,
		"app.prio() - Application priority"},
	{"confirm",(PyCFunction)cp_app_confirm, METH_VARARGS,
		"app.confirm() - Application confirmation indicator"},
	{NULL, }
};

static PyTypeObject app_pytype = {
	PyObject_HEAD_INIT(NULL)
	.tp_name = "emv.app",
	.tp_basicsize = sizeof(struct cp_app),
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_new = PyType_GenericNew,
	.tp_dealloc = (destructor)cp_app_dealloc,
	.tp_methods = cp_app_methods,
	.tp_doc = "EMV application",
};

static int cp_emv_init(struct cp_emv *self, PyObject *args, PyObject *kwds)
{
	struct cp_chipcard *cc;

	if ( !PyArg_ParseTuple(args, "O", &cc) )
		return -1;

	self->emv = emv_init(cc->slot);
	if ( NULL == self->emv ) {
		PyErr_SetString(PyExc_IOError, "emv_init() failed");
		return -1;
	}

	return 0;
}

static void cp_emv_dealloc(struct cp_emv *self)
{
	emv_fini(self->emv);
	self->ob_type->tp_free((PyObject*)self);
}

static PyObject *cp_appsel_pse(struct cp_emv *self, PyObject *args)
{
	PyObject *applist;
	emv_app_t app;

	if ( !emv_appsel_pse(self->emv) ) {
		PyErr_SetString(PyExc_IOError, "emv_appsel_pse() failed");
		return NULL;
	}

	applist = PyList_New(0);
	if ( NULL == applist )
		return NULL;

	for(app = emv_appsel_pse_first(self->emv); app;
			app = emv_appsel_pse_next(self->emv, app) ) {
		struct cp_app *a;

		a = (struct cp_app *)_PyObject_New(&app_pytype);
		if ( NULL == a )
			break;

		Py_INCREF(self);
		a->owner = self;
		a->app = app;

		PyList_Append(applist, (PyObject *)a);
	}

	return applist;
}

static PyObject *cp_select_pse(struct cp_emv *self, PyObject *args)
{
	struct cp_app *app;

	if ( !PyArg_ParseTuple(args, "O", &app) )
		return NULL;

	if ( !emv_app_select_pse(self->emv, app->app) )
		return NULL;

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *cp_select_aid(struct cp_emv *self, PyObject *args)
{
	struct cp_app *app;
	const uint8_t *aid;
	size_t len;

	if ( !PyArg_ParseTuple(args, "Os#", &app, &aid, &len) )
		return NULL;

	if ( !emv_app_select_aid(self->emv, aid, len) )
		return NULL;

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *cp_select_aid_next(struct cp_emv *self, PyObject *args)
{
	struct cp_app *app;
	const uint8_t *aid;
	size_t len;

	if ( !PyArg_ParseTuple(args, "Os#", &app, &aid, &len) )
		return NULL;

	if ( !emv_app_select_aid_next(self->emv, aid, len) )
		return NULL;

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *cp_init(struct cp_emv *self, PyObject *args)
{
	if ( !emv_app_init(self->emv) )
		return NULL;
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *data_list(struct cp_emv *owner, emv_data_t *elem,
				unsigned int nmemb)
{
	PyObject *list;
	unsigned int i;

	list = PyList_New(nmemb);
	for(i = 0; i < nmemb; i++) {
		struct cp_data *val;

		val = (struct cp_data *)_PyObject_New(&data_pytype);
		val->data = elem[i];
		val->owner = owner;
		Py_INCREF(owner);

		PyList_SetItem(list, i, (PyObject *)val);
	}

	return list;
}

static PyObject *cp_read_app_data(struct cp_emv *self, PyObject *args)
{
	emv_data_t *rec;
	unsigned int num_rec;

	if ( !emv_read_app_data(self->emv) )
		return NULL;

	rec = emv_retrieve_records(self->emv, &num_rec);
	return data_list(self, rec, num_rec);
}

static PyMethodDef cp_emv_methods[] = {
	{"appsel_pse",(PyCFunction)cp_appsel_pse, METH_VARARGS,
		"emv.appsel_pse() - Read payment system directory"},
	{"select_pse",(PyCFunction)cp_select_pse, METH_VARARGS,
		"emv.select_pse(app) - Select application from PSE entry"},
	{"select_aid",(PyCFunction)cp_select_aid, METH_VARARGS,
		"emv.select_aid(aid) - Select application from AID"},
	{"select_aid_next",(PyCFunction)cp_select_aid_next, METH_VARARGS,
		"emv.select_aid_next(aid) - Select next application from AID"},
	{"init",(PyCFunction)cp_init, METH_VARARGS,
		"emv.init() - Initiate application processing"},
	{"read_app_data",(PyCFunction)cp_read_app_data, METH_VARARGS,
		"read_app_data() - Read application data"},
	{NULL, }
};

static PyTypeObject emv_pytype = {
	PyObject_HEAD_INIT(NULL)
	.tp_name = "emv.card",
	.tp_basicsize = sizeof(struct cp_emv),
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_new = PyType_GenericNew,
	.tp_init = (initproc)cp_emv_init,
	.tp_dealloc = (destructor)cp_emv_dealloc,
	.tp_methods = cp_emv_methods,
	.tp_doc = "EMV chip",
};

static PyMethodDef methods[] = {
	{NULL, }
};

#define _INT_CONST(m, c) PyModule_AddIntConstant(m, #c, c)
PyMODINIT_FUNC initemv(void)
{
	PyObject *m;

	if ( PyType_Ready(&emv_pytype) < 0 )
		return;
	if ( PyType_Ready(&app_pytype) < 0 )
		return;
	if ( PyType_Ready(&data_pytype) < 0 )
		return;

	m = Py_InitModule3("emv", methods, "EMV chipcard API");
	if ( NULL == m )
		return;

	_INT_CONST(m, EMV_DATA_BINARY);
	_INT_CONST(m, EMV_DATA_TEXT);
	_INT_CONST(m, EMV_DATA_INT);
	_INT_CONST(m, EMV_DATA_BCD);
	_INT_CONST(m, EMV_DATA_DATE);
	_INT_CONST(m, EMV_DATA_DOL);

	Py_INCREF(&emv_pytype);
	PyModule_AddObject(m, "card", (PyObject *)&emv_pytype);
	PyModule_AddObject(m, "app", (PyObject *)&app_pytype);
	PyModule_AddObject(m, "data", (PyObject *)&data_pytype);
}
