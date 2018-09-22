#include <sys/types.h>
#include <sys/fcntl.h>

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <termios.h>
#include <err.h>
#include <string.h>

#include <libusb-1.0/libusb.h>
#include "serialio.h"

struct {
    uint16_t vid, pid;
} supported_devices[] = {
        {.vid = 0x0801, .pid = 0x0003} /* officially assigned ID */
};

libusb_device_handle *devh;
static
libusb_device_handle *open_device(void)
{
    size_t i;
    libusb_device_handle *ret = NULL;
    for (i = 0; ret == NULL &&
                i < sizeof(supported_devices) / sizeof(supported_devices[0]);
         ++i) {
        ret = libusb_open_device_with_vid_pid(NULL, supported_devices[i].vid,
                                              supported_devices[i].pid);
    }
    return ret;
}

#define USB_MAX_PACKET_SIZE 64
#define USB_MAX_PAYLOAD_SIZE 63

struct ring_buf {
    uint8_t *buf;
    size_t rx_index; /* buf[rx_index] is the next byte to return on read */
    size_t tx_index; /* buf[tx_index] is the next byte to write on append */
                     /* rx_index == tx_index when buffer is empty */
                     /* rx_index == (tx_index + 1) % size when buffer is full */
   size_t size;
};



#define BUFFER_FULL(rb) ((rb)->rx_index == ((rb)->tx_index + 1) % (rb)->size)
#define BUFFER_EMPTY(rb) ((rb)->rx_index == ((rb)->tx_index))
#define BUFFER_FILL(rb) (((rb)->tx_index - ((rb)->rx_index + (rb)->size)) % (rb)->size)
#define BUFFER_CLEAR(rb) do {(rb)->rx_index = (rb)->tx_index = 0; } while(0)

struct usb_if_state {
    struct ring_buf tx;
    struct ring_buf rx;
    uint8_t tx_buf_data [2 * (USB_MAX_PAYLOAD_SIZE + 1)];
    uint8_t rx_buf_data [USB_MAX_PAYLOAD_SIZE + 1];
} usb_if_state;

size_t ring_buf_append(struct ring_buf *rb, const void *data, size_t len) {
    size_t ret = 0;
    const uint8_t *d = (const uint8_t*)data;
    while (!BUFFER_FULL(rb) && len > ret) {
        rb->buf[rb->tx_index] = *d++;
        rb->tx_index = (rb->tx_index + 1) % rb->size;
        ++ret;
    }
    return ret;
}

size_t ring_buf_fetch(struct ring_buf *rb, void *data, size_t len) {
    size_t ret = 0;
    uint8_t *d = (uint8_t*)data;
    while (!BUFFER_EMPTY(rb) && len > ret) {
        *d++ = rb->buf[rb->rx_index];
        rb->rx_index = (rb->rx_index + 1) % rb->size;
        ++ret;
    }
    return ret;
}

/*
 * Send out what is in the send buffer and clear the receive buffer
 */

int
usb_commit (int fd)
{
    (void) fd;
    if (!BUFFER_EMPTY(&usb_if_state.tx)) {
        size_t len;
        uint8_t tmp[USB_MAX_PACKET_SIZE];
        tmp[0] = 0x80;
        do {
            len =
                    ring_buf_fetch(&usb_if_state.tx, &tmp[1], USB_MAX_PAYLOAD_SIZE);
            if (BUFFER_EMPTY(&usb_if_state.tx)) {
                tmp[0] |= 0x40;
            }
            tmp[0] |= len;
            libusb_control_transfer(devh, LIBUSB_REQUEST_TYPE_CLASS
                    | LIBUSB_RECIPIENT_INTERFACE | LIBUSB_ENDPOINT_OUT,
                    9, 0x300, 0, tmp, sizeof(tmp), 0);
        } while (!BUFFER_EMPTY(&usb_if_state.tx));
        BUFFER_CLEAR(&usb_if_state.rx);
    }
    return 0;
}
/*
 * Read a character from the serial port. Note that this
 * routine will block until a valid character is read.
 */

int
usb_readchar (int fd, uint8_t * c)
{
	uint8_t		b;
    uint8_t tmp[USB_MAX_PACKET_SIZE];
	usb_commit(fd);
	while (BUFFER_EMPTY(&usb_if_state.rx)){
	    int actual_length;
	    libusb_interrupt_transfer(devh, LIBUSB_ENDPOINT_IN | 1, tmp, sizeof(tmp), &actual_length, 0);
	    if (actual_length != sizeof(tmp)) {
	        printf("Error :-(\n");
	    }
	    ring_buf_append(&usb_if_state.rx, &tmp[1], tmp[0] & 63);
	}

	ring_buf_fetch(&usb_if_state.rx, &b, 1);
	if (c) {
	    *c = b;
	}

	return (int)b;
}

/*
 * Read a series of characters from the serial port. This
 * routine will block until the desired number of characters
 * is read.
 */

int
usb_read (int fd, void * buf, size_t len)
{
	size_t i;
	uint8_t b, *p;

	p = buf;

#ifdef SERIAL_DEBUG
	printf("[RX %.3d]", len);
#endif
	for (i = 0; i < len; i++) {
		usb_readchar (fd, &b);
#ifdef SERIAL_DEBUG
		printf(" %.2x", b);
#endif
		p[i] = b;
	}
#ifdef SERIAL_DEBUG
	printf("\n");
#endif

	return (0);
}

int
usb_write (int fd, void * buf, size_t len)
{
    (void)fd;
	return ring_buf_append(&usb_if_state.tx, buf, len);
}

int
usb_open(char *path, int * fd, int blocking, speed_t baud)
{
    int r;
    (void)path;
    (void)fd;
    (void)blocking;
    (void)baud;

    memset(&usb_if_state, 0, sizeof(usb_if_state));
    usb_if_state.rx.buf = usb_if_state.rx_buf_data;
    usb_if_state.rx.size = sizeof(usb_if_state.rx_buf_data);
    usb_if_state.tx.buf = usb_if_state.tx_buf_data;
    usb_if_state.tx.size = sizeof(usb_if_state.tx_buf_data);

    r = libusb_init(NULL);
//        libusb_set_debug(NULL, LIBUSB_LOG_LEVEL_DEBUG);
    if (r < 0) {
        fprintf(stderr, "failed to initialise libusb\n");
        exit(1);
    }

    devh = open_device();
    if (devh == NULL) {
        fprintf(stderr, "Could not find/open device\n");
        goto error;
    }

    r = libusb_set_auto_detach_kernel_driver(devh, 1);
    if (r < 0) {
        fprintf(stderr, "libusb_set_auto_detach_kernel_driver error %d (%s)\n", r, libusb_error_name(r));
        goto error;
    }

    r = libusb_claim_interface(devh, 0);
    if (r < 0) {
        fprintf(stderr, "usb_claim_interface error %d (%s)\n", r, libusb_error_name(r));
    }
    printf("claimed interface\n");

	return (0);
error:
    if (devh != NULL) {
        libusb_reset_device(devh);
    }
    libusb_close(devh);
    libusb_exit(NULL);
    return (-1);
}

int
usb_close(int fd)
{
    (void)fd;
    libusb_release_interface(devh, 0);
    libusb_close(devh);
    libusb_exit(NULL);
    return 0;
	return (0);
}

#if 1
int serial_open (char *path, int *fd, int blocking, speed_t baud)
{
    return usb_open(path, fd, blocking, baud);
}

int serial_close (int fd)
{
    return usb_close(fd);
}

int serial_readchar (int fd, uint8_t *c)
{
    return usb_readchar(fd, c);
}

int serial_write (int fd, void *data, size_t len)
{
    return usb_write(fd, data, len);
}

int serial_read (int fd, void *data, size_t len)
{
    return usb_read(fd, data, len);
}

#endif
