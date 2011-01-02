## Process this file with automake to produce Makefile.in

SUBDIRS = src

dist_pkgdata_DATA = usb-ccid-devices ins.db sw.db

EXTRA_DIST = \
	python_stuff \
	autogen.sh

python_uninstall_data = uninstall-data
python_build_dir = python-build

.PHONY: python_stuff
python_stuff:
	python setup.py build -b $(python_build_dir)

clean-local:
	rm -rf $(python_build_dir)

install-exec-hook:
	python setup.py install \
		--prefix=$(DESTDIR)/$(prefix) \
		--record=$(python_uninstall_data) \
		--install-layout=deb

uninstall-hook:
	xargs rm -f < $(python_uninstall_data)
