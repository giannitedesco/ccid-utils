## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I../include

bin_PROGRAMS = ccid

ccid_LDADD = -lusb
ccid_SOURCES = \
	util.c \
	cci-msg.c \
	chipcard.c \
	cci.c \
	main.c
