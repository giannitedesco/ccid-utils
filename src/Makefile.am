## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I../include

lib_LTLIBRARIES = libccid.la ccid.la libemv.la emv.la libsim.la
dist_bin_SCRIPTS = ccid-sh
bin_PROGRAMS = emvtool simtool

libccid_la_LIBADD = -lusb-1.0
libccid_la_SOURCES = \
	ccid-internal.h \
	util.c \
	ber.c \
	xfr.c \
	chipcard.c \
	ccidev.c \
	cci.c

ccid_la_LIBADD = -lusb-1.0 libccid.la
ccid_la_SOURCES = py_ccid.c
ccid_la_LDFLAGS = -avoid-version -export-dynamic -module \
			@PYTHON_LDADD@

libemv_la_LIBADD = libccid.la -lcrypto
libemv_la_SOURCES = 	gang.c \
			mpool.c \
			emv.c \
			emv_apdu.c \
			emv_appsel.c \
			emv_init.c \
			emv_data.c \
			emv_sda.c \
			emv_cvm.c \
			emv_trm.c \
			emv_err.c

emv_la_LIBADD = -lusb-1.0 libccid.la libemv.la
emv_la_SOURCES = py_emv.c
emv_la_LDFLAGS = -avoid-version -export-dynamic -module \
			@PYTHON_LDADD@

emvtool_LDADD = libemv.la -lusb-1.0
emvtool_SOURCES = emvtool.c

libsim_la_LIBADD = libccid.la
libsim_la_SOURCES = sim.c sim_apdu.c sim_sms.c

simtool_LDADD = libsim.la -lusb-1.0
simtool_SOURCES = simtool.c
