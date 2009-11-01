#!/bin/sh
aclocal && \
libtoolize && \
autoheader && \
automake --gnu -a -c && \
autoconf && \
test -x ./configure && ./configure $@
