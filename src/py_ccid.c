/*
 * This file is part of cci-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>
#include <ccid-spec.h>
#include <Python.h>
#include <structmember.h>

struct cp_chipcard {
	PyObject_HEAD;
	PyObject *owner;
	chipcard_t slot;
};

struct cp_cci {
	PyObject_HEAD;
	cci_t dev;
};

static PyObject *cp_chipcard_wait(struct cp_chipcard * self, PyObject *args)
{
	if ( NULL == self->slot ) {
		PyErr_SetString(PyExc_ValueError, "Bad chipcard");
		return NULL;
	}

	chipcard_wait_for_card(self->slot);

	return Py_None;
}

static PyObject *cp_chipcard_status(struct cp_chipcard * self, PyObject *args)
{
	if ( NULL == self->slot ) {
		PyErr_SetString(PyExc_ValueError, "Bad chipcard");
		return NULL;
	}

	return PyInt_FromLong(chipcard_status(self->slot));
}

static PyObject *cp_chipcard_clock(struct cp_chipcard * self, PyObject *args)
{
	long ret;

	if ( NULL == self->slot ) {
		PyErr_SetString(PyExc_ValueError, "Bad chipcard");
		return NULL;
	}

	ret = chipcard_slot_status(self->slot);
	if ( ret == CHIPCARD_CLOCK_ERR ) {
		PyErr_SetString(PyExc_IOError, "Transaction error");
		return NULL;
	}

	return PyInt_FromLong(ret);
}

static PyObject *cp_chipcard_on(struct cp_chipcard * self, PyObject *args)
{
	int voltage = CHIPCARD_AUTO_VOLTAGE;

	if ( NULL == self->slot ) {
		PyErr_SetString(PyExc_ValueError, "Bad chipcard");
		return NULL;
	}

	if ( !PyArg_ParseTuple(args, "|i", &voltage) ||
		voltage < 0 || voltage > CHIPCARD_1_8V ) {
		PyErr_SetString(PyExc_ValueError, "Bad voltage ID");
		return NULL;
	}

	if ( !chipcard_slot_on(self->slot, voltage) ) {
		PyErr_SetString(PyExc_IOError, "Transaction error");
		return NULL;
	}

	return Py_None;
}

static PyObject *cp_chipcard_off(struct cp_chipcard * self, PyObject *args)
{
	if ( NULL == self->slot ) {
		PyErr_SetString(PyExc_ValueError, "Bad chipcard");
		return NULL;
	}

	if ( !chipcard_slot_off(self->slot) ) {
		PyErr_SetString(PyExc_IOError, "Transaction error");
		return NULL;
	}

	return Py_None;
}

/* ---[ chipcard wrapper */
static PyMethodDef cp_chipcard_methods[] = {
	{"wait_for_card", (PyCFunction)cp_chipcard_wait, METH_NOARGS,	
		"chipcard.wait_for_card()\n"
		"Sleep until the end of time, or until a card is inserted."
		"whichever comes soonest."},
	{"status", (PyCFunction)cp_chipcard_status, METH_NOARGS,	
		"chipcard.status()\n"
		"Get status of the slot."},
	{"clock_status", (PyCFunction)cp_chipcard_clock, METH_NOARGS,	
		"chipcard.status()\n"
		"Get chip card clock status."},
	{"on", (PyCFunction)cp_chipcard_on, METH_VARARGS,	
		"chipcard.on(voltage=CHIPCARD_AUTO_VOLTAGE)\n"
		"Power on card and retrieve ATR."},
	{"off", (PyCFunction)cp_chipcard_off, METH_NOARGS,	
		"chipcard.off()\n"
		"Power off card."},
#if 0
	{"transact", (PyCFunction)cp_chipcard_on, METH_VARARGS,	
		"chipcard.transact() - chipcard transaction."},
#endif
	{NULL, }
};

static void cp_chipcard_dealloc(struct cp_chipcard * self)
{
	if ( self->owner ) {
		Py_DECREF(self->owner);
	}

	self->ob_type->tp_free((PyObject*)self);
}

static PyTypeObject chipcard_pytype = {
	PyObject_HEAD_INIT(NULL)
	.tp_name = "ccid.chipcard",
	.tp_basicsize = sizeof(struct cp_chipcard),
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_new = PyType_GenericNew,
	.tp_methods = cp_chipcard_methods,
	.tp_dealloc = (destructor)cp_chipcard_dealloc,
	.tp_doc = "CCI device slot",
};

/* ---[ CCI wrapper */
static PyObject *cp_get_slot(struct cp_cci *self, PyObject *args)
{
	struct cp_chipcard *cc;
	int slot;

	if (!PyArg_ParseTuple(args, "i", &slot) ) {
		PyErr_SetString(PyExc_ValueError, "Bad arguments");
		return NULL;
	}

	cc = (struct cp_chipcard*)_PyObject_New(&chipcard_pytype);
	if ( NULL == cc ) {
		PyErr_SetString(PyExc_MemoryError, "Allocating chipcard");
		return NULL;
	}

	cc->owner = (PyObject *)self;
	cc->slot = cci_get_slot(self->dev, slot);
	if ( NULL == cc->slot) {
		_PyObject_Del(cc);
		PyErr_SetString(PyExc_ValueError, "Bad slot number\n");
		return NULL;
	}

	Py_INCREF(cc->owner);
	return (PyObject *)cc;
}

static int cp_cci_init(struct cp_cci *self, PyObject *args, PyObject *kwds)
{
	ccidev_t dev;

	dev = ccid_find_first_device();
	if ( NULL == dev ) {
		PyErr_SetString(PyExc_IOError, "failed to find device");
		return -1;
	}

	self->dev = cci_probe(dev);
	if ( NULL == self->dev ) {
		PyErr_SetString(PyExc_IOError, "cci_probe() failed");
		return -1;
	}

	return 0;
}

static void cp_cci_dealloc(struct cp_cci * self)
{
	cci_close(self->dev);
	self->ob_type->tp_free((PyObject*)self);
}

static PyObject *cp_cci_slots(struct cp_cci *self, PyObject *args)
{
	return PyInt_FromLong(cci_slots(self->dev));
}

static PyMethodDef cp_cci_methods[] = {
	{"num_slots", (PyCFunction)cp_cci_slots, METH_VARARGS,	
		"cci.num_slots() - Number of CCI slots on device."},
	{"get_slot", (PyCFunction)cp_get_slot, METH_VARARGS,	
		"cci.get_slots(slot) - Number of CCI slots on device."},
	{NULL, }
};

static PyTypeObject cci_pytype = {
	PyObject_HEAD_INIT(NULL)
	.tp_name = "ccid.cci",
	.tp_basicsize = sizeof(struct cp_cci),
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_new = PyType_GenericNew,
	.tp_init = (initproc)cp_cci_init,
	.tp_dealloc = (destructor)cp_cci_dealloc,
	.tp_methods = cp_cci_methods,
	.tp_doc = "CCI device",
};

static PyObject *cp_hex_dump(PyObject *self, PyObject *args)
{
	const uint8_t *ptr;
	int len, llen = 16;
	if (!PyArg_ParseTuple(args, "s#|i", &ptr, &len, &llen))
		return NULL;
	hex_dump(ptr, len, llen);
	return Py_None;
}

static PyObject *cp_ber_dump(PyObject *self, PyObject *args)
{
	const uint8_t *ptr;
	int len;
	if (!PyArg_ParseTuple(args, "s#", &ptr, &len))
		return NULL;
	ber_dump(ptr, len, 1);
	return Py_None;
}

static PyMethodDef methods[] = {
	{"hex_dump", cp_hex_dump, METH_VARARGS,	
		"hex_dump(string, len=16) - hex dump."},
	{"ber_dump", cp_ber_dump, METH_VARARGS,
		"ber_dump(string) - dump BER TLV tags."},
	{NULL, }
};

#define _INT_CONST(m, c) PyModule_AddIntConstant(m, #c, c)
PyMODINIT_FUNC initccid(void)
{
	PyObject *m;

	if ( PyType_Ready(&cci_pytype) < 0 )
		return;
	if ( PyType_Ready(&chipcard_pytype) < 0 )
		return;

	m = Py_InitModule3("ccid", methods, "USB Chip Card Interface Driver");
	if ( NULL == m )
		return;

	_INT_CONST(m, CHIPCARD_ACTIVE);
	_INT_CONST(m, CHIPCARD_PRESENT);
	_INT_CONST(m, CHIPCARD_NOT_PRESENT);

	_INT_CONST(m, CHIPCARD_CLOCK_START);
	_INT_CONST(m, CHIPCARD_CLOCK_STOP_L);
	_INT_CONST(m, CHIPCARD_CLOCK_STOP_H);
	_INT_CONST(m, CHIPCARD_CLOCK_STOP_L);

	_INT_CONST(m, CHIPCARD_AUTO_VOLTAGE);
	_INT_CONST(m, CHIPCARD_5V);
	_INT_CONST(m, CHIPCARD_3V);
	_INT_CONST(m, CHIPCARD_1_8V);

	Py_INCREF(&cci_pytype);
	PyModule_AddObject(m, "cci", (PyObject *)&cci_pytype);

	Py_INCREF(&chipcard_pytype);
	PyModule_AddObject(m, "chipcard", (PyObject *)&chipcard_pytype);
}
