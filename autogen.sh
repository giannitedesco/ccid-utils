#!/bin/sh
aclocal && \
autoheader && \
automake --gnu -a -c && \
libtoolize && \
autoconf && \
test -x ./configure && ./configure $@
