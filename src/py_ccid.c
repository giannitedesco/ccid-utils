/*
 * This file is part of cci-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>
#include <ccid-spec.h>
#include <Python.h>
#include <structmember.h>
#include "py_ccid.h"

static ccidev_t get_dev(struct cp_dev *dev)
{
	if ( dev->dev )
		return dev->dev;
	else
		return dev->owner->list[dev->idx];
}

static PyObject *cp_dev_bus(struct cp_dev *self, PyObject *args)
{
	ccidev_t dev = get_dev(self);
	return PyInt_FromLong(ccid_device_bus(dev));
}

static PyObject *cp_dev_addr(struct cp_dev *self, PyObject *args)
{
	ccidev_t dev = get_dev(self);
	return PyInt_FromLong(ccid_device_addr(dev));
}

static int cp_dev_init(struct cp_dev *self, PyObject *args, PyObject *kwds)
{
	int bus, addr;
	ccidev_t dev;

	if ( !PyArg_ParseTuple(args, "ii", &bus, &addr) )
		return -1;
	
	dev = ccid_device(bus, addr);
	if ( NULL == dev ) {
		PyErr_SetString(PyExc_IOError, "Not a valid CCID device");
		return -1;
	}

	self->dev = dev;
	self->owner = NULL;
	return 0;
}

static void cp_dev_dealloc(struct cp_dev *self)
{
	if ( self->owner )
		Py_DECREF(self->owner);
	self->ob_type->tp_free((PyObject*)self);
}

static PyMethodDef cp_dev_methods[] = {
	{"bus", (PyCFunction)cp_dev_bus, METH_NOARGS,	
		"xfr.bus()\n"
		"Retrieves bus number of device."},
	{"addr", (PyCFunction)cp_dev_addr, METH_NOARGS,	
		"xfr.addr()\n"
		"Retrieves USB device address."},
	{NULL, }
};

static PyTypeObject dev_pytype = {
	PyObject_HEAD_INIT(NULL)
	.tp_name = "ccid.dev",
	.tp_basicsize = sizeof(struct cp_dev),
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_new = PyType_GenericNew,
	.tp_init = (initproc)cp_dev_init,
	.tp_dealloc = (destructor)cp_dev_dealloc,
	.tp_methods = cp_dev_methods,
	.tp_doc = "CCI device list entry",
};

static int cp_devlist_init(struct cp_devlist *self, PyObject *args,
				PyObject *kwds)
{
	self->list = ccid_get_device_list(&self->nmemb);
	return 0;
}

static void cp_devlist_dealloc(struct cp_devlist *self)
{
	ccid_free_device_list(self->list);
	self->ob_type->tp_free((PyObject*)self);
}

static Py_ssize_t devlist_len(struct cp_devlist *self)
{
	return self->nmemb;
}

static PyObject *devlist_get(struct cp_devlist *self, Py_ssize_t i)
{
	struct cp_dev *dev;

	if ( i >= self->nmemb )
		return NULL;

	dev = (struct cp_dev*)_PyObject_New(&dev_pytype);
	if ( dev ) {
		Py_INCREF(self);
		dev->owner = self;
		dev->idx = i;
		dev->dev = NULL;
	}
	return (PyObject *)dev;
}

static PySequenceMethods devlist_seq = {
	.sq_length = (lenfunc)devlist_len,
	.sq_item = (ssizeargfunc)devlist_get,
};

static PyTypeObject devlist_pytype = {
	PyObject_HEAD_INIT(NULL)
	.tp_name = "ccid.devlist",
	.tp_basicsize = sizeof(struct cp_devlist),
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_new = PyType_GenericNew,
	.tp_init = (initproc)cp_devlist_init,
	.tp_dealloc = (destructor)cp_devlist_dealloc,
	.tp_as_sequence = &devlist_seq,
	.tp_doc = "CCI device list",
};

/* ---[ Xfr wrapper */
static int cp_xfr_init(struct cp_xfr *self, PyObject *args, PyObject *kwds)
{
	int tx, rx;

	if ( !PyArg_ParseTuple(args, "ii", &tx, &rx) )
		return -1;

	self->xfr = xfr_alloc(tx, rx);
	if ( NULL == self->xfr ) {
		PyErr_SetString(PyExc_MemoryError, "Allocating buffer");
		return -1;
	}

	return 0;
}

static void cp_xfr_dealloc(struct cp_xfr *self)
{
	xfr_free(self->xfr);
	self->ob_type->tp_free((PyObject*)self);
}

static PyObject *cp_xfr_reset(struct cp_xfr *self, PyObject *args)
{
	xfr_reset(self->xfr);
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *cp_xfr_byte(struct cp_xfr *self, PyObject *args)
{
	uint8_t byte;

	if ( !PyArg_ParseTuple(args, "b", &byte) )
		return NULL;

	if ( !xfr_tx_byte(self->xfr, byte) ) {
		PyErr_SetString(PyExc_MemoryError, "TX buffer overflow");
		return NULL;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *cp_xfr_str(struct cp_xfr *self, PyObject *args)
{
	const uint8_t *str;
	int len;

	if ( !PyArg_ParseTuple(args, "s#", &str, &len) )
		return NULL;

	if ( !xfr_tx_buf(self->xfr, str, len) ) {
		PyErr_SetString(PyExc_MemoryError, "TX buffer overflow");
		return NULL;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *cp_xfr_sw1(struct cp_xfr *self, PyObject *args)
{
	return PyInt_FromLong(xfr_rx_sw1(self->xfr));
}

static PyObject *cp_xfr_sw2(struct cp_xfr *self, PyObject *args)
{
	return PyInt_FromLong(xfr_rx_sw2(self->xfr));
}

static PyObject *cp_xfr_data(struct cp_xfr *self, PyObject *args)
{
	const uint8_t *str;
	size_t len;
	
	str = xfr_rx_data(self->xfr, &len);
	if ( NULL == str ) {
		PyErr_SetString(PyExc_ValueError, "RX buffer underflow");
		return NULL;
	}
	return Py_BuildValue("s#", str, (int)len);
}

static PyMethodDef cp_xfr_methods[] = {
	{"reset", (PyCFunction)cp_xfr_reset, METH_NOARGS,	
		"xfr.reset()\n"
		"Reset transmit and receive buffers."},
	{"tx_byte", (PyCFunction)cp_xfr_byte, METH_VARARGS,
		"xfr.tx_byte()\n"
		"Push a byte to the transmit buffer."},
	{"tx_str", (PyCFunction)cp_xfr_str, METH_VARARGS,
		"xfr.tx_str()\n"
		"Push a string to the transmit buffer."},
	{"rx_sw1", (PyCFunction)cp_xfr_sw1, METH_NOARGS,
		"xfr.rx_sw1()\n"
		"Retrieve SW1 status word."},
	{"rx_sw2", (PyCFunction)cp_xfr_sw2, METH_NOARGS,
		"xfr.rx_sw1()\n"
		"Retrieve SW2 status word."},
	{"rx_data", (PyCFunction)cp_xfr_data, METH_NOARGS,
		"xfr.rx_data()\n"
		"Return receive buffer data."},
	{NULL,}
};

static PyTypeObject xfr_pytype = {
	PyObject_HEAD_INIT(NULL)
	.tp_name = "ccid.xfr",
	.tp_basicsize = sizeof(struct cp_xfr),
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_new = PyType_GenericNew,
	.tp_methods = cp_xfr_methods,
	.tp_init = (initproc)cp_xfr_init,
	.tp_dealloc = (destructor)cp_xfr_dealloc,
	.tp_doc = "CCI transfer buffer",
};

/* ---[ chipcard wrapper */
static PyObject *cp_chipcard_transact(struct cp_chipcard *self, PyObject *args)
{
	struct cp_xfr *xfr;

	if ( NULL == self->slot ) {
		PyErr_SetString(PyExc_ValueError, "Bad slot");
		return NULL;
	}

	if ( !PyArg_ParseTuple(args, "O", &xfr) )
		return NULL;

	if ( xfr->ob_type != &xfr_pytype ) {
		PyErr_SetString(PyExc_TypeError, "Not an xfr object");
		return NULL;
	}

	if ( !chipcard_transact(self->slot, xfr->xfr) ) {
		PyErr_SetString(PyExc_IOError, "Transaction error");
		return NULL;
	}

	return PyInt_FromLong(xfr_rx_sw1(xfr->xfr));
}

static PyObject *cp_chipcard_wait(struct cp_chipcard *self, PyObject *args)
{
	if ( NULL == self->slot ) {
		PyErr_SetString(PyExc_ValueError, "Bad slot");
		return NULL;
	}

	chipcard_wait_for_card(self->slot);

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *cp_chipcard_status(struct cp_chipcard *self, PyObject *args)
{
	if ( NULL == self->slot ) {
		PyErr_SetString(PyExc_ValueError, "Bad slot");
		return NULL;
	}

	return PyInt_FromLong(chipcard_slot_status(self->slot));
}

static PyObject *cp_chipcard_clock(struct cp_chipcard *self, PyObject *args)
{
	long ret;

	if ( NULL == self->slot ) {
		PyErr_SetString(PyExc_ValueError, "Bad slot");
		return NULL;
	}

	ret = chipcard_clock_status(self->slot);
	if ( ret == CHIPCARD_CLOCK_ERR ) {
		PyErr_SetString(PyExc_IOError, "Transaction error");
		return NULL;
	}

	return PyInt_FromLong(ret);
}

static PyObject *cp_chipcard_on(struct cp_chipcard *self, PyObject *args)
{
	int voltage = CHIPCARD_AUTO_VOLTAGE;
	const uint8_t *ptr;
	size_t atr_len;

	if ( NULL == self->slot ) {
		PyErr_SetString(PyExc_ValueError, "Bad slot");
		return NULL;
	}

	if ( !PyArg_ParseTuple(args, "|i", &voltage) )
		return NULL;

	if ( voltage < 0 || voltage > CHIPCARD_1_8V ) {
		PyErr_SetString(PyExc_ValueError, "Bad voltage ID");
		return NULL;
	}

	ptr = chipcard_slot_on(self->slot, voltage, &atr_len);
	if ( NULL == ptr ) {
		PyErr_SetString(PyExc_IOError, "Transaction error");
		return NULL;
	}

	return Py_BuildValue("s#", ptr, (int)atr_len);
}

static PyObject *cp_chipcard_off(struct cp_chipcard *self, PyObject *args)
{
	if ( NULL == self->slot ) {
		PyErr_SetString(PyExc_ValueError, "Bad slot");
		return NULL;
	}

	if ( !chipcard_slot_off(self->slot) ) {
		PyErr_SetString(PyExc_IOError, "Transaction error");
		return NULL;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyMethodDef cp_chipcard_methods[] = {
	{"wait_for_card", (PyCFunction)cp_chipcard_wait, METH_NOARGS,	
		"slot.wait_for_card()\n"
		"Sleep until the end of time, or until a card is inserted."
		"whichever comes soonest."},
	{"status", (PyCFunction)cp_chipcard_status, METH_NOARGS,	
		"slot.status()\n"
		"Get status of the slot."},
	{"clock_status", (PyCFunction)cp_chipcard_clock, METH_NOARGS,	
		"slot.status()\n"
		"Get chip card clock status."},
	{"on", (PyCFunction)cp_chipcard_on, METH_VARARGS,	
		"slotchipcard.on(voltage=CHIPCARD_AUTO_VOLTAGE)\n"
		"Power on card and retrieve ATR."},
	{"off", (PyCFunction)cp_chipcard_off, METH_NOARGS,	
		"slot.off()\n"
		"Power off card."},
	{"transact", (PyCFunction)cp_chipcard_transact, METH_VARARGS,	
		"slot.transact(xfr) - chipcard transaction."},
	{NULL, }
};

static void cp_chipcard_dealloc(struct cp_chipcard *self)
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
static PyObject *cci_get(struct cp_cci *self, Py_ssize_t i)
{
	struct cp_chipcard *cc;

	cc = (struct cp_chipcard*)_PyObject_New(&chipcard_pytype);
	if ( NULL == cc ) {
		PyErr_SetString(PyExc_MemoryError, "Allocating chipcard");
		return NULL;
	}

	cc->owner = (PyObject *)self;
	cc->slot = cci_get_slot(self->dev, i);
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
	const char *trace = NULL;
	struct cp_dev *cpd;
	ccidev_t dev;

	if ( !PyArg_ParseTuple(args, "O|z", &cpd, &trace) )
		return -1;

	if ( cpd->ob_type != &dev_pytype ) {
		PyErr_SetString(PyExc_TypeError, "Expected valid cci.dev");
		return -1;
	}

	dev = get_dev(cpd);

	self->dev = cci_probe(dev, trace);
	if ( NULL == self->dev ) {
		PyErr_SetString(PyExc_IOError, "cci_probe() failed");
		return -1;
	}

	return 0;
}

static void cp_cci_dealloc(struct cp_cci *self)
{
	cci_close(self->dev);
	self->ob_type->tp_free((PyObject*)self);
}

static Py_ssize_t cci_len(struct cp_cci *self)
{
	return cci_slots(self->dev);
}

static PyObject *cp_log(struct cp_cci *self, PyObject *args)
{
	const char *str;

	if ( !PyArg_ParseTuple(args, "s", &str) )
		return NULL;
	
	cci_log(self->dev, "%s", str);

	Py_INCREF(Py_None);
	return Py_None;
}

static PyMethodDef cp_cci_methods[] = {
	{"log",(PyCFunction)cp_log, METH_VARARGS,
		"cci.log(string) - Log some text to the tracefile"},
	{NULL, }
};

static PySequenceMethods cci_seq = {
	.sq_length = (lenfunc)cci_len,
	.sq_item = (ssizeargfunc)cci_get,
};

static PyTypeObject cci_pytype = {
	PyObject_HEAD_INIT(NULL)
	.tp_name = "ccid.cci",
	.tp_basicsize = sizeof(struct cp_cci),
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_new = PyType_GenericNew,
	.tp_init = (initproc)cp_cci_init,
	.tp_dealloc = (destructor)cp_cci_dealloc,
	.tp_as_sequence = &cci_seq,
	.tp_methods = cp_cci_methods,
	.tp_doc = "CCI device",
};

static PyObject *cp_hex_dump(PyObject *self, PyObject *args)
{
	const uint8_t *ptr;
	int len, llen = 16;
	if ( !PyArg_ParseTuple(args, "s#|i", &ptr, &len, &llen) )
		return NULL;
	hex_dump(ptr, len, llen);
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *cp_ber_dump(PyObject *self, PyObject *args)
{
	const uint8_t *ptr;
	int len;
	if ( !PyArg_ParseTuple(args, "s#", &ptr, &len) )
		return NULL;
	ber_dump(ptr, len, 1);
	Py_INCREF(Py_None);
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

	if ( PyType_Ready(&dev_pytype) < 0 )
		return;
	if ( PyType_Ready(&devlist_pytype) < 0 )
		return;
	if ( PyType_Ready(&xfr_pytype) < 0 )
		return;
	if ( PyType_Ready(&chipcard_pytype) < 0 )
		return;
	if ( PyType_Ready(&cci_pytype) < 0 )
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

	Py_INCREF(&xfr_pytype);
	PyModule_AddObject(m, "xfr", (PyObject *)&xfr_pytype);

	Py_INCREF(&chipcard_pytype);
	PyModule_AddObject(m, "slot", (PyObject *)&chipcard_pytype);

	Py_INCREF(&cci_pytype);
	PyModule_AddObject(m, "cci", (PyObject *)&cci_pytype);

	Py_INCREF(&dev_pytype);
	PyModule_AddObject(m, "dev", (PyObject *)&dev_pytype);

	Py_INCREF(&devlist_pytype);
	PyModule_AddObject(m, "devlist", (PyObject *)&devlist_pytype);
}
