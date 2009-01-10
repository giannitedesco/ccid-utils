## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I../include

bin_PROGRAMS = scsh

lib_LTLIBRARIES = libccid.la ccid.la

libccid_la_LIBADD = -lusb
libccid_la_SOURCES = \
	util.c \
	ber.c \
	emv.c \
	gsm.c \
	xfr.c \
	chipcard.c \
	dev.c \
	cci.c

ccid_la_LIBADD = -lusb libccid.la
ccid_la_SOURCES = py_ccid.c
ccid_la_LDFLAGS = -avoid-version -export-dynamic -module \
			@PYTHON_LDADD@

scsh_LDADD = libccid.la -lusb
scsh_SOURCES = main.c
