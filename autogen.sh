#!/bin/sh
aclocal && \
autoheader && \
automake --gnu -a -c && \
autoconf && \
test -x ./configure && ./configure $@
