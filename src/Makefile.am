## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I../include

lib_LTLIBRARIES = libccid.la ccid.la libemv.la
dist_bin_SCRIPTS = ccid-sh
bin_PROGRAMS = emvtool simtool

libccid_la_LIBADD = -lusb
libccid_la_SOURCES = \
	ccid-internals.h \
	util.c \
	ber.c \
	xfr.c \
	chipcard.c \
	dev.c \
	cci.c

ccid_la_LIBADD = -lusb libccid.la
ccid_la_SOURCES = py_ccid.c
ccid_la_LDFLAGS = -avoid-version -export-dynamic -module \
			@PYTHON_LDADD@
libemv_la_LIBADD = libccid.la -lcrypto
libemv_la_SOURCES = emv.c visa.c link.c emv_sda.c

emvtool_LDADD = libemv.la libccid.la -lusb
emvtool_SOURCES = emvtool.c

simtool_LDADD = libccid.la -lusb
simtool_SOURCES = simtool.c sim.c sim.h
