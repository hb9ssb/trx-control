/*
 * Copyright (c) 2023 - 2025 Marc Balmer HB9SSB
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

#include <err.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>

static void
usage(void)
{
	(void)fprintf(stderr, "usage: bluecat [-V] [-c channel] device\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct sockaddr_rc addr = { 0 };
	int ch, fd, channel, status;
	char c;

	channel = 0;

	while (1) {
		int option_index = 0;
		static struct option long_options[] = {
			{ "channel",		required_argument, 0, 'c' },
			{ "version",		no_argument, 0, 'V' },
			{ 0, 0, 0, 0 }
		};

		ch = getopt_long(argc, argv, "c:", long_options,
		    &option_index);

		if (ch == -1)
			break;

		switch (ch) {
		case 0:
			break;
		case 'c':
			channel = atoi(optarg);
			break;
		case 'V':
			printf("bluecat %s\n", VERSION);
			exit(0);
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	fd = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
	addr.rc_family = AF_BLUETOOTH;
	addr.rc_channel = (uint8_t) channel;
	str2ba(argv[0], &addr.rc_bdaddr);

	status = connect(fd, (struct sockaddr *)&addr, sizeof(addr));

	if (status)
		err(1, "can't connect to %s on channel %d", argv[0], channel);

	do {
		status = read(fd, &c, 1);

		if (status == 1) {
			putchar(c);
			fflush(stdout);
		}
	} while (status > 0);

	return 0;
}
