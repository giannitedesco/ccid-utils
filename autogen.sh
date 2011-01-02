#!/bin/sh
aclocal && \
libtoolize -c && \
autoheader && \
automake --gnu -a -c && \
autoconf && \
test -x ./configure && ./configure $@
