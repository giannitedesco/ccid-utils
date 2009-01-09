## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I../include

bin_PROGRAMS = ccid

ccid_LDADD = -lusb
ccid_SOURCES = \
	util.c \
	ber.c \
	emv.c \
	gsm.c \
	xfr.c \
	chipcard.c \
	cci.c \
	main.c
