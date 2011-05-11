/* hack fd passing to give shell access from user one user to another */
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <fcntl.h>

#include <ccid.h>
static ccid_t ccid;
static cci_t cci;
static xfr_t xfr;

#define SOCKET_FN "/tmp/osmocom_simctl"

static int fdctl_coe(int fd, int coe)
{
	int ret;

	if ( coe )
		coe = FD_CLOEXEC;
	else
		coe = 0;

	ret = fcntl(fd, F_SETFD, coe);
	if ( ret < 0 )
		return 0;

	return 1;
}

static const char *sys_err(void)
{
	return strerror(errno);
}

static const char *cmd;

static int get_msg(int s)
{
	const uint8_t *rx;
	uint8_t buf[1024];
	uint8_t *ptr;
	ssize_t rc;
	size_t sz;

	xfr_reset(xfr);

	rc = recv(s, buf, sizeof(buf), 0);
	if ( rc <= 0 )
		return 0;

	printf("Retrieved %zu bytes of data\n", (size_t)rc);
	xfr_tx_buf(xfr, buf, rc);
	if ( !cci_transact(cci, xfr) ) {
		fprintf(stderr, "cci_transact error %s\n", sys_err());
		exit(EXIT_FAILURE);
	}

	rx = xfr_rx_data(xfr, &sz);
	ptr = malloc(sz + 2);
	memcpy(ptr, rx, sz);
	ptr[sz + 0] = xfr_rx_sw1(xfr);
	ptr[sz + 1] = xfr_rx_sw2(xfr);
	
	rc = send(s, ptr, sz + 2, 0);
	free(ptr);
	if ( rc <= 0 )
		return 0;
	
	printf("Sent %zu bytes of data\n", sz + 2);
	return 1;
}

static int found_ccid(ccidev_t dev, int bus, int addr)
{
	static unsigned int count;
	unsigned int i, num_slots;
	char fn[128];
	int ret = 0;
	
	printf("Found CCI device at %d.%d\n",
		libccid_device_bus(dev),
		libccid_device_addr(dev));

	if ( bus < 0 || addr < 0 )
		return 0;
	
	if ( libccid_device_bus(dev) != bus )
		return 0;
	if ( libccid_device_addr(dev) != addr )
		return 0;

	snprintf(fn, sizeof(fn), "simctl.%u.trace", count);
	ccid = ccid_probe(dev, fn);
	if ( NULL == ccid )
		goto out;

	count++;
	printf("%s\n", ccid_name(ccid));

	num_slots = ccid_num_slots(ccid);
	printf("CCID has %d slots\n", num_slots);
	for(i = 0; i < num_slots; i++) {
		printf(" Probing slot %d\n", i);
		cci = ccid_get_slot(ccid, i);
		return 1;
	}

	ret = 0;
	ccid_close(ccid);
out:
	return ret;
}

static int open_ccid(int bus, int addr)
{
	ccidev_t *dev;
	size_t num_dev, i;

	dev = libccid_get_device_list(&num_dev);
	if ( NULL == dev )
		return 0;

	for(i = 0; i < num_dev; i++) {
		if ( found_ccid(dev[i], bus, addr) ) {
			xfr = xfr_alloc(1024, 1024);
			if ( NULL == xfr )
				return 0;
			return 1;
		}
	}

	libccid_free_device_list(dev);

	return 0;
}

int main(int argc, char **argv)
{
	struct sockaddr_un sa;
	socklen_t salen;
	int s, c;

	cmd = argv[0];

	if ( argc < 3 ) {
		open_ccid(-1, -1);
		return EXIT_SUCCESS;
	}

	if ( !open_ccid(atoi(argv[1]), atoi(argv[2])) ) {
		fprintf(stderr, "Suitable device not found\n");
		return EXIT_FAILURE;
	}

	if ( !cci_power_on(cci, CHIPCARD_AUTO_VOLTAGE, NULL) ) {
		fprintf(stderr, "Failed to retreive ATR\n");
		return EXIT_FAILURE;
	}

	s = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if ( s < 0 ) {
		fprintf(stderr, "%s: socket: %s\n", cmd, sys_err());
		return EXIT_FAILURE;
	}

	if ( !fdctl_coe(s, 1) ) {
		fprintf(stderr, "%s: fdctl: %s\n", cmd, sys_err());
		return EXIT_FAILURE;
	}

	if ( unlink(SOCKET_FN) && errno != ENOENT ) {
		fprintf(stderr, "%s: unlink: %s\n", cmd, sys_err());
		return EXIT_FAILURE;
	}

	sa.sun_family = AF_UNIX;
	snprintf(sa.sun_path, sizeof(sa.sun_path), "%s", SOCKET_FN);
	if ( bind(s, (struct sockaddr *)&sa, sizeof(sa)) ) {
		fprintf(stderr, "%s: unlink: %s\n", cmd, sys_err());
		return EXIT_FAILURE;
	}
	
	if ( listen(s, 1) ) {
		fprintf(stderr, "%s: listen: %s\n", cmd, sys_err());
		return EXIT_FAILURE;
	}

	printf("Accepting connections on: %s\n", SOCKET_FN);

	salen = sizeof(sa);
	c = accept(s, (struct sockaddr *)&sa, &salen);
	if ( c < 0 ) {
		fprintf(stderr, "%s: accept: %s\n", cmd, sys_err());
		return EXIT_FAILURE;
	}

	if ( !fdctl_coe(c, 1) ) {
		fprintf(stderr, "%s: fdctl: %s\n", cmd, sys_err());
		return EXIT_FAILURE;
	}

	for(; get_msg(c); )
		/* nothing */;

	return EXIT_SUCCESS;
}
