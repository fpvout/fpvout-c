/*
 * Copyright 2021 Google LLC.
 * SPDX-License-Identifier: Apache-2.0
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <ctype.h>
#include <libusb-1.0/libusb.h>

#define USB_VID 0x2ca3
#define USB_PID 0x1f

#define USB_CONFIG 1
#define USB_INTERFACE 3
#define USB_ENDPOINT_VIDEO_IN 0x84
#define USB_ENDPOINT_CONTROL_OUT 0x03
size_t USB_BUFFER_SIZE_BYTES = 1024;

size_t PENDING_TRANSFERS_MAX = 512;

libusb_context *ctx;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
void* handle_events(void* args) {
#pragma GCC diagnostic pop
	for (;;) {
		libusb_handle_events(ctx);
	}
}

static void read_callback(struct libusb_transfer *transfer) {
	if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-result"
		write(1/*stdout*/, transfer->buffer, transfer->actual_length);
#pragma GCC diagnostic pop

	}
	libusb_submit_transfer(transfer);
}

static struct libusb_transfer* create_transfer(libusb_device_handle *dev, size_t length) {
	uint8_t *buf = malloc(length);
	struct libusb_transfer *transfer = libusb_alloc_transfer(0);
	libusb_fill_bulk_transfer(transfer,
		dev,
		USB_ENDPOINT_VIDEO_IN,
		buf,
		length,
		read_callback,
		NULL,
		5000);
	return transfer;
}

int main(int argc, char** argv)
{
	PENDING_TRANSFERS_MAX = 1024;
	USB_BUFFER_SIZE_BYTES = 1024 * 64;
	int c;
	while ((c = getopt (argc, argv, "p:s:")) != -1) {
		switch (c)
		{
			case 'p':
				PENDING_TRANSFERS_MAX = atoi(optarg);
				break;
			case 's':
				USB_BUFFER_SIZE_BYTES = atoi(optarg);
				break;
			case '?':
				if (optopt == 'p' || optopt == 's') {
					fprintf (stderr, "Option -%c requires an argument.\n", optopt);
				} else if (isprint (optopt)) {
					fprintf (stderr, "Unknown option `-%c'.\n", optopt);
				} else {
					fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
				}
				return 1;
			default:
				abort();
		}
	}
	fprintf(stderr, "allocating %zu pending transfers of up to %zu bytes\n", PENDING_TRANSFERS_MAX, USB_BUFFER_SIZE_BYTES);

	int r = libusb_init(&ctx);
	if (r < 0) {
		fprintf(stderr, "unable to init libusb: %s\n", libusb_strerror(r));
		exit(1);
	}

	// Get device handle
	libusb_device_handle *dev = libusb_open_device_with_vid_pid(NULL, USB_VID, USB_PID);
	if (dev == NULL){
		libusb_exit(NULL);
		fprintf(stderr, "unable to open device, or device not found\n");
		if (geteuid() != 0) {
			fprintf(stderr, "try running as root (with sudo)\n");
		}
		exit(1);
	}

	// Detach the kernel (all non-fatal)
	libusb_reset_device(dev);
	// Detach kernel driver (RNDIS)
	libusb_detach_kernel_driver(dev, 0);
	// Detach kernel driver (Storage)
	libusb_detach_kernel_driver(dev, 2);
	// Detach kernel driver (Storage)
	libusb_detach_kernel_driver(dev, 4);

	// Set active configuration
	r = libusb_set_configuration(dev, USB_CONFIG);
	if (r != 0) {
		fprintf(stderr, "unable to set configuration: (%d) %s\n", r, libusb_strerror(r));
		exit(1);
	}

	// Claim interface
	r = libusb_claim_interface(dev, USB_INTERFACE);
	if (r != 0) {
		fprintf(stderr, "unable to claim interface: %s\n", libusb_strerror(r));
		exit(1);
	}

	// Send magic
	unsigned char MAGIC[] = { 0x52, 0x4d, 0x56, 0x54 };
	int MAGIC_LENGTH = 4;
	unsigned int MAGIC_TIMEOUT_MS = 500;
	r = libusb_bulk_transfer(dev, USB_ENDPOINT_CONTROL_OUT, MAGIC, MAGIC_LENGTH, NULL, MAGIC_TIMEOUT_MS);
	if (r != 0 && r != LIBUSB_ERROR_TIMEOUT) {
		fprintf(stderr, "unable to send magic: %s\n", libusb_strerror(r));
		exit(1);
	}

	fprintf(stderr, "%s ready\n", argv[0]);
	for (size_t i = 0; i < PENDING_TRANSFERS_MAX; i++) {
		libusb_submit_transfer(create_transfer(dev, USB_BUFFER_SIZE_BYTES));
	}
	// Loooooooooooop
	pthread_t events_thread;
	pthread_create(&events_thread, NULL, handle_events, ctx);
	// We'll never reach this point, but let's clean up anyway
	libusb_exit(NULL);
	// There's technically some buffers malloc'd that we're not freeing... But we're exiting anyway.
	return 0;
}
