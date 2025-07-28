/*
 * Copyright (c) 2025 Marc Balmer HB9SSB
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/* Handle systemd events */

#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>

#include <systemd/sd-event.h>
#include <systemd/sd-device.h>

#include "trxd.h"
#include "trx-control.h"

extern int verbose;

static void
cleanup(void *arg)
{
	if (verbose)
		syslog(LOG_NOTICE, "sd-handler: terminating\n");
}

static int
device_monitor(sd_device_monitor *sddm, sd_device *device, void *arg)
{
	sd_event *event;
	sd_device_action_t action;
	const char *devname;

	/* For the moment only output information in verbose mode */
	if (!verbose)
		return 0;

	event = sd_device_monitor_get_event(sddm);

	sd_device_get_action(device, &action);
	switch (action) {
	case SD_DEVICE_ADD:
		printf("SD_DEVICE_ADD\n");
		break;
	case SD_DEVICE_REMOVE:
		printf("SD_DEVICE_REMOVE\n");
		break;
	case SD_DEVICE_CHANGE:
		printf("SD_DEVICE_CHANGE\n");
		break;
	case SD_DEVICE_MOVE:
		printf("SD_DEVICE_MOVE\n");
		break;
	case SD_DEVICE_ONLINE:
		printf("SD_DEVICE_ONLINE\n");
		break;
	case SD_DEVICE_OFFLINE:
		printf("SD_DEVICE_OFFLINE\n");
		break;
	case SD_DEVICE_BIND:
		printf("SD_DEVICEBIND\n");
		break;
	case SD_DEVICE_UNBIND:
		printf("SD_DEVICE_UNBIND\n");
		break;
	default:
		printf("UNKNOWN ACTION\n");
	}
	printf("device-monitor\n");
	if (sd_device_get_devname(device, &devname) >= 0)
		printf("devname: %s\n", devname);
	if (sd_device_get_devpath(device, &devname) >= 0)
		printf("devpath: %s\n", devname);
	if (sd_device_get_subsystem(device, &devname) >= 0)
		printf("subsystem: %s\n", devname);
	if (sd_device_get_syspath(device, &devname) >= 0)
		printf("syspath: %s\n", devname);
	if (sd_device_get_sysname(device, &devname) >= 0)
		printf("sysname: %s\n", devname);
	if (sd_device_get_devtype(device, &devname) >= 0)
		printf("devtype: %s\n", devname);
	if (sd_device_get_driver(device, &devname) >= 0)
		printf("driver: %s\n", devname);
	if (sd_device_get_property_value(device, "ID_MODEL", &devname) >= 0)
		printf("ID_MODEL: %s\n", devname);
	printf("\n");
	return 0;
}

void *
sd_event_handler(void *arg)
{
	sd_event *event = NULL;
	sd_device_monitor *sddm = NULL;

	if (pthread_detach(pthread_self())) {
		syslog(LOG_ERR, "sd-event-handler: pthread_detach");
		exit(1);
	}

	pthread_cleanup_push(cleanup, NULL);

	if (pthread_setname_np(pthread_self(), "sd-event")) {
		syslog(LOG_ERR,"sd-event-handler: pthread_setname_np");
		exit(1);
	}

	sd_device_monitor_new(&sddm);
	sd_device_monitor_filter_add_match_subsystem_devtype(sddm, "usb",
	    "usb_device");
	sd_device_monitor_filter_add_match_subsystem_devtype(sddm, "tty", NULL);
	sd_device_monitor_start(sddm, device_monitor, NULL);

	event = sd_device_monitor_get_event(sddm);
	sd_event_loop(event);

	pthread_cleanup_pop(0);
	return NULL;
}
