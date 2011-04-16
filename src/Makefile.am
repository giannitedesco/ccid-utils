## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I../include

lib_LTLIBRARIES = libccid.la libemv.la libsim.la
dist_bin_SCRIPTS = ccid-sh gemv
bin_PROGRAMS = emvtool simtool cselect

libccid_la_LIBADD = -lusb-1.0
libccid_la_LDFLAGS =  -version-info 4:0:0
libccid_la_SOURCES = \
	ccid-internal.h \
	rfid-internal.h \
	cci_contact.c \
	rfid_layer1.c \
	rfid_layer1.h \
	cci_rfid.c \
	proto_tcl.c \
	proto_tcl.h \
	iso14443a.c \
	iso14443a.h \
	clrc632.c \
	clrc632.h \
	omnikey.c \
	ccidev.c \
	rfid.c \
	rfid.h \
	ccid.c \
	cci.c \
	util.c \
	ber.c \
	xfr.c

libemv_la_LIBADD = libccid.la -lcrypto
libemv_la_LDFLAGS =  -version-info 4:0:0
libemv_la_SOURCES = 	gang.c \
			mpool.c \
			emv.c \
			emv_apdu.c \
			emv_appsel.c \
			emv_init.c \
			emv_data.c \
			emv_sda.c \
			emv_dda.c \
			emv_cvm.c \
			emv_trm.c \
			emv_err.c

emvtool_LDADD = libemv.la -lusb-1.0
emvtool_SOURCES = emvtool.c

libsim_la_LIBADD = libccid.la
libsim_la_LDFLAGS =  -version-info 4:0:0
libsim_la_SOURCES = sim.c sim_apdu.c sim_sms.c

simtool_LDADD = libsim.la -lusb-1.0
simtool_SOURCES = simtool.c

cselect_LDADD = libccid.la
cselect_SOURCES = cselect.c
