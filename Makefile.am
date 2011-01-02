## Process this file with automake to produce Makefile.in

SUBDIRS = src

EXTRA_DIST = \
	python_stuff \
	autogen.sh

.PHONY: python_stuff
python_stuff:
	python setup.py  build -b src/

python_uninstall_data := uninstall-data
install-exec-hook:
	python setup.py install \
		--prefix=$(DESTDIR) \
		--record=$(python_uninstall_data)

uninstall-hook:
	xargs rm -f < $(python_uninstall_data)
