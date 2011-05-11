/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <Python.h>
#include <ccid.h>
#include <ccid-spec.h>
#include <structmember.h>
#include "py_ccid.h"

#define MODNAME "ccid"

static PyObject *_ccid_err;

static ccidev_t get_dev(struct cp_dev *dev)
{
	if ( dev->dev )
		return dev->dev;
	else
		return dev->owner->list[dev->idx];
}

static PyObject *cp_dev_bus(struct cp_dev *self)
{
	ccidev_t dev = get_dev(self);
	return PyInt_FromLong(libccid_device_bus(dev));
}

static PyObject *cp_dev_addr(struct cp_dev *self)
{
	ccidev_t dev = get_dev(self);
	return PyInt_FromLong(libccid_device_addr(dev));
}

static int cp_dev_init(struct cp_dev *self, PyObject *args, PyObject *kwds)
{
	int bus, addr;
	ccidev_t dev;

	if ( !PyArg_ParseTuple(args, "ii", &bus, &addr) )
		return -1;
	
	dev = libccid_device_by_address(bus, addr);
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

static PyGetSetDef cp_dev_attribs[] = {
	{"bus", (getter)cp_dev_bus, NULL,	
		"xfr.bus()\n"
		"Retrieves bus number of device."},
	{"addr", (getter)cp_dev_addr, NULL,
		"xfr.addr()\n"
		"Retrieves USB device address."},
	{NULL, }
};

static PyTypeObject dev_pytype = {
	PyObject_HEAD_INIT(NULL)
	.tp_name = MODNAME ".dev",
	.tp_basicsize = sizeof(struct cp_dev),
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_new = PyType_GenericNew,
	.tp_init = (initproc)cp_dev_init,
	.tp_dealloc = (destructor)cp_dev_dealloc,
	.tp_getset = cp_dev_attribs,
	.tp_doc = "CCI device list entry",
};

static int cp_devlist_init(struct cp_devlist *self, PyObject *args,
				PyObject *kwds)
{
	self->list = libccid_get_device_list(&self->nmemb);
	return 0;
}

static void cp_devlist_dealloc(struct cp_devlist *self)
{
	libccid_free_device_list(self->list);
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
	.tp_name = MODNAME ".devlist",
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
	.tp_name = MODNAME ".xfr",
	.tp_basicsize = sizeof(struct cp_xfr),
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_new = PyType_GenericNew,
	.tp_methods = cp_xfr_methods,
	.tp_init = (initproc)cp_xfr_init,
	.tp_dealloc = (destructor)cp_xfr_dealloc,
	.tp_doc = "CCI transfer buffer",
};

/* ---[ chipcard wrapper */
static PyObject *cp_cci_transact(struct cp_cci *self, PyObject *args)
{
	struct cp_xfr *xfr;

	if ( NULL == self->slot ) {
		PyErr_SetString(_ccid_err, "Bad slot");
		return NULL;
	}

	if ( !PyArg_ParseTuple(args, "O", &xfr) )
		return NULL;

	if ( xfr->ob_type != &xfr_pytype ) {
		PyErr_SetString(PyExc_TypeError, "Not an xfr object");
		return NULL;
	}

	if ( !cci_transact(self->slot, xfr->xfr) ) {
		PyErr_SetString(PyExc_IOError, "Transaction error");
		return NULL;
	}

	return PyInt_FromLong(xfr_rx_sw1(xfr->xfr));
}

static PyObject *cp_cci_wait(struct cp_cci *self, PyObject *args)
{
	if ( NULL == self->slot ) {
		PyErr_SetString(_ccid_err, "Bad slot");
		return NULL;
	}

	cci_wait_for_card(self->slot);

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *cp_cci_status(struct cp_cci *self)
{
	if ( NULL == self->slot ) {
		PyErr_SetString(_ccid_err, "Bad slot");
		return NULL;
	}

	return PyInt_FromLong(cci_slot_status(self->slot));
}

static PyObject *cp_cci_clock(struct cp_cci *self, PyObject *args)
{
	long ret;

	if ( NULL == self->slot ) {
		PyErr_SetString(_ccid_err, "Bad slot");
		return NULL;
	}

	ret = cci_clock_status(self->slot);
	if ( ret == CHIPCARD_CLOCK_ERR ) {
		PyErr_SetString(PyExc_IOError, "Transaction error");
		return NULL;
	}

	return PyInt_FromLong(ret);
}

static PyObject *cp_cci_on(struct cp_cci *self, PyObject *args)
{
	int voltage = CHIPCARD_AUTO_VOLTAGE;
	const uint8_t *ptr;
	size_t atr_len;

	if ( NULL == self->slot ) {
		PyErr_SetString(_ccid_err, "Bad slot");
		return NULL;
	}

	if ( !PyArg_ParseTuple(args, "|i", &voltage) )
		return NULL;

	if ( voltage < 0 || voltage > CHIPCARD_1_8V ) {
		PyErr_SetString(_ccid_err, "Bad voltage ID");
		return NULL;
	}

	ptr = cci_power_on(self->slot, voltage, &atr_len);
	if ( NULL == ptr ) {
		PyErr_SetString(PyExc_IOError, "Transaction error");
		return NULL;
	}

	return Py_BuildValue("s#", ptr, (int)atr_len);
}

static PyObject *cp_cci_off(struct cp_cci *self, PyObject *args)
{
	if ( NULL == self->slot ) {
		PyErr_SetString(_ccid_err, "Bad slot");
		return NULL;
	}

	if ( !cci_power_off(self->slot) ) {
		PyErr_SetString(PyExc_IOError, "Transaction error");
		return NULL;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyMethodDef cp_cci_methods[] = {
	{"wait_for_card", (PyCFunction)cp_cci_wait, METH_NOARGS,	
		"cci.wait_for_card()\n"
		"Sleep until the end of time, or until a card is inserted."
		"whichever comes soonest."},
	{"on", (PyCFunction)cp_cci_on, METH_VARARGS,	
		"slotchipcard.on(voltage=CHIPCARD_AUTO_VOLTAGE)\n"
		"Power on card and retrieve ATR."},
	{"off", (PyCFunction)cp_cci_off, METH_NOARGS,	
		"cci.off()\n"
		"Power off card."},
	{"transact", (PyCFunction)cp_cci_transact, METH_VARARGS,	
		"cci.transact(xfr) - chipcard transaction."},
	{NULL, }
};

static PyMethodDef cp_slot_methods[] = {
	{"clock_status", (PyCFunction)cp_cci_clock, METH_NOARGS,	
		"cci.clock_status()\n"
		"Get chip card clock status."},
	{NULL, }
};

static void cp_cci_dealloc(struct cp_cci *self)
{
	if ( self->owner ) {
		Py_DECREF(self->owner);
	}

	self->ob_type->tp_free((PyObject*)self);
}

static PyObject *cci_index_get(struct cp_cci *self)
{
	return PyInt_FromLong(self->index);
}

static PyObject *cci_owner_get(struct cp_cci *self)
{
	Py_INCREF(self->owner);
	return self->owner;
}

static PyGetSetDef cp_cci_attribs[] = {
	{"index", (getter)cci_index_get, NULL,
		"Interface index"},
	{"owner", (getter)cci_owner_get, NULL,
		"Owning device"},
	{"status", (getter)cp_cci_status, NULL,	
		"cci.status()\n"
		"Get card-present status of the interface."},
	{NULL,}
};

static PyTypeObject cci_pytype = {
	PyObject_HEAD_INIT(NULL)
	.tp_name = MODNAME ".cci",
	.tp_basicsize = sizeof(struct cp_cci),
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	.tp_new = PyType_GenericNew,
	.tp_methods = cp_cci_methods,
	.tp_getset = cp_cci_attribs,
	.tp_dealloc = (destructor)cp_cci_dealloc,
	.tp_doc = "Chip Card Interface",
};

static PyTypeObject slot_pytype = {
	PyObject_HEAD_INIT(NULL)
	.tp_name = MODNAME ".slot",
	.tp_base = &cci_pytype,
	.tp_basicsize = sizeof(struct cp_cci),
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	.tp_new = PyType_GenericNew,
	.tp_dealloc = (destructor)cp_cci_dealloc,
	.tp_methods = cp_slot_methods,
	.tp_doc = "Contact Card Interface",
};

static PyTypeObject rfid_pytype = {
	PyObject_HEAD_INIT(NULL)
	.tp_name = MODNAME".rfid",
	.tp_base = &cci_pytype,
	.tp_basicsize = sizeof(struct cp_cci),
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	.tp_new = PyType_GenericNew,
	.tp_dealloc = (destructor)cp_cci_dealloc,
	.tp_doc = "Proximity Card Interface",
};

/* ---[ CCI wrapper */
static PyObject *Py_cci_New(PyTypeObject *type,
				struct cp_ccid *owner, cci_t cci,
				unsigned int idx)
{
	struct cp_cci *cc;

	cc = (struct cp_cci *)_PyObject_New(type);
	if ( NULL == cc ) {
		PyErr_SetString(PyExc_MemoryError,
				"Allocating chipcard interface");
		return NULL;
	}

	cc->owner = (PyObject *)owner;
	cc->slot = cci;
	cc->index = idx;

	Py_INCREF(cc->owner);
	return (PyObject *)cc;
}

static int cp_ccid_init(struct cp_ccid *self, PyObject *args, PyObject *kwds)
{
	const char *trace = NULL;
	struct cp_dev *cpd;
	ccidev_t dev;

	if ( !PyArg_ParseTuple(args, "O|z", &cpd, &trace) )
		return -1;

	if ( cpd->ob_type != &dev_pytype ) {
		PyErr_SetString(PyExc_TypeError, "Expected valid ccid.dev");
		return -1;
	}

	dev = get_dev(cpd);

	self->dev = ccid_probe(dev, trace);
	if ( NULL == self->dev ) {
		PyErr_SetString(PyExc_IOError, "ccid_probe() failed");
		return -1;
	}

	return 0;
}

static void cp_ccid_dealloc(struct cp_ccid *self)
{
	ccid_close(self->dev);
	self->ob_type->tp_free((PyObject*)self);
}

static PyObject *ccid_interfaces_get(struct cp_ccid *self)
{
	size_t num_slots, num_fields, i;
	PyObject *list, *elem;
	cci_t cci;

	num_slots = ccid_num_slots(self->dev);
	num_fields = ccid_num_fields(self->dev);

	list = PyList_New(num_slots + num_fields);
	if ( NULL == list )
		return NULL;

	for(i = 0; i < num_slots; i++) {
		cci = ccid_get_slot(self->dev, i);
		if ( NULL == cci )
			goto err;

		elem = Py_cci_New(&slot_pytype, self, cci, i);
		if ( NULL == elem )
			goto err;
		PyList_SetItem(list, i, elem);
	}

	for(i = 0; i < num_fields; i++) {
		cci = ccid_get_field(self->dev, i);
		if ( NULL == cci )
			goto err;

		elem = Py_cci_New(&rfid_pytype, self, cci, i);
		if ( NULL == elem )
			goto err;
		PyList_SetItem(list, num_slots + i, elem);
	}

	return list;
err:
	Py_DECREF(list);
	return NULL;
}

static PyObject *cp_log(struct cp_ccid *self, PyObject *args)
{
	const char *str;

	if ( !PyArg_ParseTuple(args, "s", &str) )
		return NULL;
	
	ccid_log(self->dev, "%s", str);

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *ccid_bus_get(struct cp_ccid *self)
{
	return PyInt_FromLong(ccid_bus(self->dev));
}

static PyObject *ccid_addr_get(struct cp_ccid *self)
{
	return PyInt_FromLong(ccid_addr(self->dev));
}

static PyObject *ccid_name_get(struct cp_ccid *self)
{
	return PyString_FromString(ccid_name(self->dev));
}

static PyGetSetDef cp_ccid_attribs[] = {
	{"interfaces", (getter)ccid_interfaces_get, NULL,
		"Chipcard Interfaces"},
	{"bus", (getter)ccid_bus_get, NULL,
		"Chipcard Interfaces"},
	{"addr", (getter)ccid_addr_get, NULL,
		"Chipcard Interfaces"},
	{"name", (getter)ccid_name_get, NULL,
		"Chipcard Interfaces"},
	{NULL, }
};

static PyMethodDef cp_ccid_methods[] = {
	{"log",(PyCFunction)cp_log, METH_VARARGS,
		MODNAME ".log(string) - Log some text to the tracefile"},
	{NULL, }
};

static PyTypeObject ccid_pytype = {
	PyObject_HEAD_INIT(NULL)
	.tp_name = MODNAME ".ccid",
	.tp_basicsize = sizeof(struct cp_ccid),
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_new = PyType_GenericNew,
	.tp_init = (initproc)cp_ccid_init,
	.tp_dealloc = (destructor)cp_ccid_dealloc,
	.tp_methods = cp_ccid_methods,
	.tp_getset = cp_ccid_attribs,
	.tp_doc = "Chipcard Interface Device",
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
	if ( PyType_Ready(&ccid_pytype) < 0 )
		return;
	if ( PyType_Ready(&cci_pytype) < 0 )
		return;
	if ( PyType_Ready(&slot_pytype) < 0 )
		return;
	if ( PyType_Ready(&rfid_pytype) < 0 )
		return;

	m = Py_InitModule3(MODNAME, methods, "USB Chip Card Interface Driver");
	if ( NULL == m )
		return;

	_INT_CONST(m, CHIPCARD_ACTIVE);
	_INT_CONST(m, CHIPCARD_PRESENT);
	_INT_CONST(m, CHIPCARD_NOT_PRESENT);

	_INT_CONST(m, CHIPCARD_CLOCK_START);
	_INT_CONST(m, CHIPCARD_CLOCK_STOP);
	_INT_CONST(m, CHIPCARD_CLOCK_STOP_L);
	_INT_CONST(m, CHIPCARD_CLOCK_STOP_H);
	_INT_CONST(m, CHIPCARD_CLOCK_STOP_L);

	_INT_CONST(m, CHIPCARD_AUTO_VOLTAGE);
	_INT_CONST(m, CHIPCARD_5V);
	_INT_CONST(m, CHIPCARD_3V);
	_INT_CONST(m, CHIPCARD_1_8V);

	_ccid_err = PyErr_NewException(MODNAME ".CCID_Error",
					PyExc_Exception, NULL);
	Py_INCREF(_ccid_err);
	PyModule_AddObject(m, "CCID_Error", _ccid_err);

	Py_INCREF(&xfr_pytype);
	PyModule_AddObject(m, "xfr", (PyObject *)&xfr_pytype);

	Py_INCREF(&cci_pytype);
	PyModule_AddObject(m, "cci", (PyObject *)&cci_pytype);

	Py_INCREF(&slot_pytype);
	PyModule_AddObject(m, "slot", (PyObject *)&slot_pytype);

	Py_INCREF(&rfid_pytype);
	PyModule_AddObject(m, "rfid", (PyObject *)&rfid_pytype);

	Py_INCREF(&ccid_pytype);
	PyModule_AddObject(m, "ccid", (PyObject *)&ccid_pytype);

	Py_INCREF(&dev_pytype);
	PyModule_AddObject(m, "dev", (PyObject *)&dev_pytype);

	Py_INCREF(&devlist_pytype);
	PyModule_AddObject(m, "devlist", (PyObject *)&devlist_pytype);
}
