/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>

#include "ccid-internal.h"

static libusb_context *ctx;
static void do_init(void)
{
	if ( NULL == ctx )
		libusb_init(&ctx);
}

static int check_interface(struct libusb_device *dev, int c, int i, int generic)
{
	struct libusb_config_descriptor *conf;
	const struct libusb_interface *iface;
	int a, ret;

	if ( libusb_get_config_descriptor(dev, c, &conf) )
		return -1;

	iface = &conf->interface[i];

	/* Scan for generic CCID class */
	for (ret = a = 0; a < iface->num_altsetting; a++) {
		const struct libusb_interface_descriptor *id =
							&iface->altsetting[a];
		if ( id->bInterfaceClass == 0x0b ) {
			return a;
		}
	}
	
	if ( generic )
		goto out;

	/* If no generic CCID found, try first proprietary since the vendor
	 * and device ID's are in our list
	 */
	for (ret = a = 0; a < iface->num_altsetting; a++) {
		const struct libusb_interface_descriptor *id =
							&iface->altsetting[a];
		if ( id->bInterfaceClass == 0xff ) {
			return a;
		}
	}
out:
	return -1;
}

static int check_vendor_dev_list(uint16_t idVendor, uint16_t idProduct)
{
	if ( idVendor == 0x4e6 && idProduct == 0xe003 ) {
		return 1;
	}

	return 0;
}

/** Probe a USB device for a CCID interface.
 * @param dev \ref ccidev_t representing a physical device.
 * @param cp Pointer to an integer to retrieve configuration number.
 * @param ip Pointer to an integer to retrieve interface number.
 * @param ap Pointer to an integer to retrieve alternate setting number.
 *
 * @return zero indicates failure.
 */
int _probe_descriptors(struct libusb_device *dev, int *cp, int *ip, int *ap)
{
	struct libusb_device_descriptor d;
	int c, i, a, supported;

	c = i = a = 0;

	if ( libusb_get_device_descriptor(dev, &d) )
		return 0;

	supported = check_vendor_dev_list(d.idVendor, d.idProduct);

	for(c = 0; c < d.bNumConfigurations; c++) {
		struct libusb_config_descriptor *conf;
		if ( libusb_get_config_descriptor(dev, c, &conf) )
			return 0;
		for(i = 0; i < conf->bNumInterfaces; i++) {
			a = check_interface(dev, c, i, !supported);
			if ( a < 0 )
				continue;

			goto success;
		}
	}

	return 0;

success:
	c++;

	if ( cp )
		*cp = c;
	if ( ip )
		*ip = i;
	if ( ap )
		*ap = a;
	return 1;
}

/** Find first physical CCI device on the system.
 * \ingroup g_ccid
 *
 * @return Handle for the first device. Actually a libusb libusb_device pointer.
 */
ccidev_t *ccid_get_device_list(size_t *nmemb)
{
	libusb_device **devlist;
	ccidev_t *ccilist;
	ssize_t numdev, i, n;

	do_init();

	numdev = libusb_get_device_list(ctx, &devlist);
	if ( numdev <= 0 ) {
		*nmemb = 0;
		return NULL;
	}

	ccilist = calloc(numdev + 1, sizeof(*ccilist));
	for(i = n = 0; i < numdev; i++) {
		if ( !_probe_descriptors(devlist[i], NULL, NULL, NULL) )
			continue;
		libusb_ref_device(devlist[i]);
		ccilist[n++] = devlist[i];
	}
	ccilist[n] = NULL;

	libusb_free_device_list(devlist, 1);

	*nmemb = n;
	return ccilist;
}

void ccid_free_device_list(ccidev_t *list)
{
	if ( list ) {
		ccidev_t *ptr;
		for(ptr = list; *ptr; ptr++)
			libusb_unref_device(*ptr);
		free(list);
	}
}

ccidev_t ccid_device(uint8_t bus, uint8_t addr)
{
	libusb_device **devlist;
	ssize_t numdev, i;
	ccidev_t ret;

	do_init();

	numdev = libusb_get_device_list(ctx, &devlist);
	if ( numdev <= 0 )
		return NULL;

	for(ret = NULL, i = 0; i < numdev; i++) {
		if ( libusb_get_bus_number(devlist[i]) != bus )
			continue;
		if ( libusb_get_device_address(devlist[i]) != addr )
			continue;
		if ( _probe_descriptors(devlist[i], NULL, NULL, NULL) ) {
			ret = devlist[i];
			libusb_ref_device(ret);
		}
		break;
	}

	libusb_free_device_list(devlist, 1);

	return ret;
}

uint8_t ccid_device_bus(ccidev_t dev)
{
	return libusb_get_bus_number(dev);
}

uint8_t ccid_device_addr(ccidev_t dev)
{
	return libusb_get_device_address(dev);
}
