/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>
#include <stdio.h>

#include "ccid-internal.h"

static int check_interface(struct usb_device *dev, int c, int i)
{
	struct usb_interface *iface = &dev->config[c].interface[i];
	int a, ret;

	for (ret = a = 0; a < iface->num_altsetting; a++) {
		struct usb_interface_descriptor *id = &iface->altsetting[a];
		if ( id->bInterfaceClass == 0x0b )
			return a;
	}

	return -1;
}

int _probe_device(struct usb_device *dev, int *cp, int *ip, int *ap)
{
	int c, i, a;

	c = i = a = 0;

	if ( dev->descriptor.idVendor == 0x4e6 &&
		dev->descriptor.idProduct == 0xe003 ) {
		c = 1;
			goto success;
	}

	for(c = 0; c < dev->descriptor.bNumConfigurations; c++) {
		for(i = 0; i < dev->config[c].bNumInterfaces; i++) {
			a = check_interface(dev, c, i);
			if ( a < 0 )
				continue;

			goto success;
		}
	}

	return 0;

success:
	if ( cp )
		*cp = c;
	if ( ip )
		*ip = i;
	if ( ap )
		*ap = a;
	return 1;
}

ccidev_t ccid_find_first_device(void)
{
	struct usb_bus *bus, *busses;
	struct usb_device *dev;

	usb_init();
	usb_find_busses();
	usb_find_devices();

	busses = usb_get_busses();

	for(bus = busses; bus; bus = bus->next) {
		for(dev = bus->devices; dev; dev = dev->next)
			if ( _probe_device(dev, NULL, NULL, NULL) )
				return dev;
	}

	return NULL;
}
