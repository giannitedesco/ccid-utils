/*
 * This file is part of ccid-utils
 * Copyright (c) 2008 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#include <ccid.h>
#include <ctype.h>
#include <errno.h>

#include "ccid-internal.h"

static libusb_context *ctx;

struct devid {
	uint16_t idVendor;
	uint16_t idProduct;
	unsigned int flags;
	char *name;
};

#define DEVID_ALLOC_CHUNK (1<<5)
#define DEVID_ALLOC_MASK  (DEVID_ALLOC_CHUNK - 1)
static struct devid *devid;
static unsigned int num_devid;

/* Easy string tokeniser */
static int easy_explode(char *str, char split,
			char **toks, int max_toks)
{
	char *tmp;
	int tok;
	int state;

	for(tmp=str,state=tok=0; *tmp && tok <= max_toks; tmp++) {
		if ( state == 0 ) {
			if ( *tmp == split && (tok < max_toks)) {
				toks[tok++] = NULL;
			}else if ( !isspace(*tmp) ) {
				state = 1;
				toks[tok++] = tmp;
			}
		}else if ( state == 1 ) {
			if ( tok < max_toks ) {
				if ( *tmp == split || isspace(*tmp) ) {
					*tmp = '\0';
					state = 0;
				}
			}else if ( *tmp == '\n' )
				*tmp = '\0';
		}
	}

	return tok;
}

static int strtou16(const char *s, uint16_t *v)
{
	unsigned long int ret;
	char *end = NULL;

	ret=strtoul(s, &end, 0);

	if ( end == s )
		return -1;

	if ( ret == ULONG_MAX && errno == ERANGE )
		return -1;

	if ( ret > UINT_MAX )
		return -1;
	if ( ret & ~0xffff )
		return 0;

	*v = ret;
	if ( *end == '\0' )
		return 0;

	return end - s;
}

static int devid_assure(struct devid **d, unsigned int cnt)
{
	static void *new;

	if ( cnt & DEVID_ALLOC_MASK )
		return 1;

	new = realloc(*d, sizeof(**d) * (cnt + DEVID_ALLOC_CHUNK));
	if ( new == NULL )
		return 0;

	*d = new;
	return 1;
}

static int append_devid(struct devid **d, unsigned int *cnt,
			uint16_t idProduct, uint16_t idVendor,
			unsigned int flags, const char *str)
{
	char *tmp = strdup(str);

	if ( NULL == tmp )
		return 0;

	if ( !devid_assure(d, *cnt) ) {
		free(tmp);
		return 0;
	}

	(*d)[*cnt].idProduct = idProduct;
	(*d)[*cnt].idVendor = idVendor;
	(*d)[*cnt].flags = flags;
	(*d)[*cnt].name = tmp;
	(*cnt)++;
	return 1;
}

static unsigned int parse_flags(const char *fn, unsigned int line,
				char *buf)
{
	unsigned int ret;
	char *tok[32];
	int ntok, i;

	ntok = easy_explode(buf, '|', tok, sizeof(tok)/sizeof(*tok));
	if ( ntok < 1 )
		return 0;
	
	for(ret = i = 0; i < ntok; i++) {
		if ( !strcmp(tok[i], "RFID_OMNI") ) {
			ret |= INTF_RFID_OMNI;
		}else{
			fprintf(stderr, "%s:%u: '%s' unknown device flag\n",
				fn, line, tok[i]);
		}
	}

	return ret;
}

static struct devid *load_device_types(unsigned int *pcnt)
{
	static const char * const fn =
			CCID_UTILS_DATADIR "/" PACKAGE "/usb-ccid-devices";
	FILE *f;
	char buf[512];
	char *tok[4];
	unsigned int cnt, line;
	struct devid *ret;

	f = fopen(fn, "r");
	if ( NULL == f ) {
		fprintf(stderr, "%s: open: %s\n", fn, strerror(errno));
		return NULL;
	}
	
	for(line = 1, cnt = 0, ret = NULL; fgets(buf, sizeof(buf), f); line++) {
		uint16_t idVendor, idProduct;
		unsigned int flags = 0;
		int num_toks;
		char *lf;

		if ( buf[0] == '#' || buf[0] == '\r' || buf[0] == '\n' )
			continue;

		lf = strchr(buf, '\n');
		if ( NULL == lf ) {
			fprintf(stderr,
				"%s:%u: line exceeded max line length (%u)",
				fn, line, sizeof(buf));
			break;
		}

		*lf = '\0';

		num_toks = easy_explode(buf, ':',
					tok, sizeof(tok)/sizeof(*tok));
		switch(num_toks) {
		case 0:
		case 1:
		default:
			fprintf(stderr, "%s:%u: badly formed line", fn, line);
			continue;
		case 2:
			tok[2] = NULL;
		case 3:
			tok[3] = NULL;
		case 4:
			break;
		}

		if ( strtou16(tok[0], &idVendor) ||
			strtou16(tok[1], &idProduct ) ) {
			fprintf(stderr, "%s:%u: bad ID number", fn, line);
			continue;
		}
		if ( tok[2] && strlen(tok[2]) )
			flags = parse_flags(fn, line, tok[2]);
		append_devid(&ret, &cnt, idProduct, idVendor, flags, tok[3]);
	}

	fclose(f);
	*pcnt = cnt;
	return ret;
}

static void zap_device_types(struct devid *d, unsigned int num)
{
	unsigned int i;
	for(i = 0; i < num; i++)
		free(d[i].name);
	free(d);
}

static void reload_device_types(void)
{
	struct devid *tmp;
	unsigned int count;

	tmp = load_device_types(&count);
	if ( NULL == tmp )
		return;

	zap_device_types(devid, num_devid);
	devid = tmp;
	num_devid = count;
}

static void do_init(void)
{
	if ( NULL == ctx )
		libusb_init(&ctx);
	reload_device_types();
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

static struct devid *check_vendor_dev_list(uint16_t idVendor,
						uint16_t idProduct)
{
	unsigned int i;

	for(i = 0; i < num_devid; i++) {
		if ( devid[i].idVendor == idVendor &&
			devid[i].idProduct == idProduct ) {
			return devid + i;
		}
	}

	return NULL;
}

/** Probe a USB device for a CCID interface.
 * @param dev \ref ccidev_t representing a physical device.
 * @param intf struct containting interface parameters
 *
 * @return zero indicates failure.
 */
int _probe_descriptors(struct libusb_device *dev, struct _cci_interface *intf)
{
	struct libusb_device_descriptor d;
	struct devid *id;
	int c, i, a;

	c = i = a = 0;

	if ( libusb_get_device_descriptor(dev, &d) )
		return 0;

	id = check_vendor_dev_list(d.idVendor, d.idProduct);

	for(c = 0; c < d.bNumConfigurations; c++) {
		struct libusb_config_descriptor *conf;
		if ( libusb_get_config_descriptor(dev, c, &conf) )
			return 0;
		for(i = 0; i < conf->bNumInterfaces; i++) {
			a = check_interface(dev, c, i, id == NULL);
			if ( a < 0 )
				continue;

			goto success;
		}
	}

	return 0;

success:
	c++;

	if ( intf ) {
		intf->c = c;
		intf->i = i;
		intf->a = a;
		intf->flags = id->flags;
		intf->name = id->name;
	}
	return 1;
}

/** Find first physical CCI device on the system.
 * \ingroup g_ccid
 *
 * @return Handle for the first device. Actually a libusb libusb_device pointer.
 */
ccidev_t *libccid_get_device_list(size_t *nmemb)
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
		if ( !_probe_descriptors(devlist[i], NULL) )
			continue;
		libusb_ref_device(devlist[i]);
		ccilist[n++] = devlist[i];
	}
	ccilist[n] = NULL;

	libusb_free_device_list(devlist, 1);

	*nmemb = n;
	return ccilist;
}

void libccid_free_device_list(ccidev_t *list)
{
	if ( list ) {
		ccidev_t *ptr;
		for(ptr = list; *ptr; ptr++)
			libusb_unref_device(*ptr);
		free(list);
	}
}

ccidev_t libccid_device_by_address(uint8_t bus, uint8_t addr)
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
		if ( _probe_descriptors(devlist[i], NULL) ) {
			ret = devlist[i];
			libusb_ref_device(ret);
		}
		break;
	}

	libusb_free_device_list(devlist, 1);

	return ret;
}

uint8_t libccid_device_bus(ccidev_t dev)
{
	return libusb_get_bus_number(dev);
}

uint8_t libccid_device_addr(ccidev_t dev)
{
	return libusb_get_device_address(dev);
}
