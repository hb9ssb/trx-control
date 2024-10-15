/*
 * Copyright (c) 2023 - 2024 Marc Balmer HB9SSB
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

/* Provide the 'trx' Lua module to transceiver drivers */

#include <err.h>
#include <poll.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include <lua.h>
#include <lauxlib.h>

#include "trxd.h"

extern __thread int cat_device;
extern int verbose;

static int
luatrx_version(lua_State *L)
{
	lua_pushstring(L, TRXD_VERSION);
	return 1;
}

static int
luatrx_wait_for_data(lua_State *L)
{
	struct pollfd pfd;
	int timeout;
	size_t nfds;

	if (lua_gettop(L) == 1)
		timeout = luaL_checkinteger(L, 1);
	else
		timeout = -1;	/* Infinite timeout */

	pfd.fd = cat_device;
	pfd.events = POLLIN;

	nfds = poll(&pfd, 1, timeout);
	if (nfds == -1)
		return luaL_error(L, "poll error");

	lua_pushboolean(L, nfds == 1 ? 1 : 0);

	return 1;
}

static int
luatrx_read(lua_State *L)
{
	struct pollfd pfd;
	char buf[256];
	size_t len, nread, nfds;

	len = luaL_checkinteger(L, 1);

	pfd.fd = cat_device;
	pfd.events = POLLIN;

	nread = 0;
	if (verbose > 1)
		printf("<- (read %d bytes from %d)\n", len, cat_device);
	while (nread < len) {
		nfds = poll(&pfd, 1, 100);
		if (nfds == -1)
			return luaL_error(L, "poll error");
		if (nfds == 1) {
			nread += read(cat_device, &buf[nread], len - nread);
		} else {
			if (verbose > 1)
				printf("timeout\n");
			break;
		}
	}

	if (nread > 0 && verbose > 1) {
		int i;

		for (i = 0; i < nread; i++)
			printf("%02X ", buf[i]);
		printf("\n");
	}

	if (nread > 0)
		lua_pushlstring(L, buf, nread);
	else
		lua_pushnil(L);

	if (verbose > 1)
		printf("\n");
	return 1;
}

static int
luatrx_write(lua_State *L)
{
	const char *data;
	size_t len;

	data = luaL_checklstring(L, 1, &len);
	tcflush(cat_device, TCIFLUSH);
	if (verbose > 1) {
		int i;

		printf("-> ");
		for (i = 0; i < len; i++)
			printf("%02X ", data[i]);
		printf("\n");
	}
	write(cat_device, data, len);
	tcdrain(cat_device);
	return 0;
}

static int
bcd_to_string(lua_State *L)
{
	unsigned char *bcd_string, *string, *p;
	size_t len;
	int n;

	bcd_string = (unsigned char *)luaL_checklstring(L, 1, &len);
	string = p = malloc(len * 2 + 1);
	for (n = 0; n < len; n++, bcd_string++) {
		*p++ = '0' + (*bcd_string >> 4);
		*p++ = '0' + (*bcd_string & 0x0f);
	}
	*p = '\0';
	lua_pushstring(L, string);
	free(string);
	return 1;
}

static int
string_to_bcd(lua_State *L)
{
	unsigned char *bcd_string, *string, *p;
	size_t len;
	int n;

	string = (unsigned char *)luaL_checklstring(L, 1, &len);
	bcd_string = p = malloc(len);
	for (n = 0; n < len / 2; n++)
		*p++ = *string++ - '0' << 4 | *string++ & 0x0f;
	*p = '\0';

	lua_pushlstring(L, bcd_string, len / 2);
	free(bcd_string);
	return 1;
}

static int
crc16(lua_State *L)
{
	void const *data;
	size_t len, i;
	uint16_t x, crc;
	const uint8_t *buf;
	unsigned char crc_out[2];

	x = 0;
	crc = 0x1d0f;
	data = luaL_checklstring(L, 1, &len);
	buf = ((const uint8_t *) data);

	for(i = 0; i < len; i++) {
		x = (crc >> 8) ^ buf[i];
		x ^= x >> 4;
		crc = (crc << 8) ^ (x << 12) ^ (x << 5) ^ x;
	}
	crc_out[0] = crc & 0xff;
	crc_out[1] = (crc >> 8) & 0xff;
	lua_pushlstring(L, crc_out, 2);
	return 1;
}

static int
luatrx_verbose(lua_State *L)
{
	lua_pushinteger(L, verbose);
	return 1;
}

int
luaopen_trx(lua_State *L)
{
	struct luaL_Reg luatrx[] = {
		{ "version",		luatrx_version },
		{ "read",		luatrx_read },
		{ "write",		luatrx_write },
		{ "waitForData",	luatrx_wait_for_data },
		{ "bcdToString",	bcd_to_string },
		{ "stringToBcd",	string_to_bcd },
		{ "crc16",		crc16 },
		{ "verbose",		luatrx_verbose },
		{ NULL, NULL }
	};

	luaL_newlib(L, luatrx);
	lua_pushliteral(L, "_COPYRIGHT");
	lua_pushliteral(L, "Copyright (c) 2023 - 2024 Marc Balmer HB9SSB");
	lua_settable(L, -3);
	lua_pushliteral(L, "_DESCRIPTION");
	lua_pushliteral(L, "trx-control for Lua");
	lua_settable(L, -3);
	lua_pushliteral(L, "_VERSION");
	lua_pushliteral(L, "trx " TRXD_VERSION);
	lua_settable(L, -3);

	return 1;
}
