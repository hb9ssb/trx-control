/*
 * Copyright (c) 2023 Micro Systems Marc Balmer, CH-5073 Gipf-Oberfrick
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* network access extension module  */

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>

#include <errno.h>
#ifdef LIBFETCH
#include <fetch.h>
#endif
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "luasocket.h"

static ssize_t
to_read(int fd, void *buf, size_t len, int ms)
{
	struct pollfd p;
	int r;

	p.fd = fd;
	p.events = POLLIN;
	p.revents = 0;

	if ((r = poll(&p, 1, ms)) > 0)
		r = read(fd, buf, len);
	return r;
}

/* XXX factor out common code */
static int
asreadln(int fd, char **ret, int ms)
{
	struct pollfd p;
	int r, n;
	size_t len, nread;
	char *s;

	p.fd = fd;
	p.events = POLLIN;
	p.revents = 0;

	*ret = malloc(NET_BUFSIZ);
	if (*ret == NULL)
		return -1;
	len = NET_BUFSIZ;
	nread = 0;
	s = *ret;
	do {
		n = 0;
		r = poll(&p, 1, ms);
		switch (r) {
		case 0:
			fprintf(stderr, "poll timeout\n");
			break;
		case -1:
			fprintf(stderr, "poll error\n");
			break;
		default:
			if (len - nread > 0) {
				/* read one character at a time */
				n = read(fd, s, 1);
				nread += n;
				s += n;
			} else {
				len *= 2;
				*ret = realloc(*ret, len);
				if (*ret == NULL)
					fprintf(stderr, "memory error\n");
				s = *ret + nread;
				n = 1;
			}
		}
	} while (n > 0 && (*ret)[nread - 1] != '\n');
	(*ret)[nread - 1] = '\0';
	return nread;
}

static int
luanet_accept(lua_State *L)
{
	struct sockaddr_storage addr;
	socklen_t len = sizeof(addr);
	int s, *data;

	s = accept(*(int *)luaL_checkudata(L, 1, SOCKET_METATABLE),
	    (struct sockaddr *)&addr, &len);
	if (s == -1) {
		lua_pushnil(L);
		return 1;
	}
	data = (int *)lua_newuserdata(L, sizeof(int *));
	*data = s;
	luaL_getmetatable(L, SOCKET_METATABLE);
	lua_setmetatable(L, -2);
	return 1;
}

static int
luanet_bind(lua_State *L)
{
	struct addrinfo hints, *res, *res0;
	struct sockaddr_un addr;
	int fd, error, *data;
	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
	const char *port, *host;

	host = luaL_checkstring(L, 1);

	if (*host == '/' || *host == '.') {
		fd = socket(AF_UNIX, SOCK_STREAM, 0);

		if (fd < 0)
			return luaL_error(L, "connection error");

		memset(&addr, 0, sizeof(struct sockaddr_un));
		addr.sun_family = AF_UNIX;
		strncpy(addr.sun_path, host, sizeof(addr.sun_path) - 1);

		if (bind(fd, (struct sockaddr *)&addr,
		    sizeof(struct sockaddr_un)) == -1) {
		    	close(fd);
			return luaL_error(L, "bind error");
		}

		if (listen(fd, lua_gettop(L) > 1 ? luaL_checkinteger(L, 2) : 32))
			return luaL_error(L, "listen error");

	} else {
		port = luaL_checkstring(L, 2);

		memset(&hints, 0, sizeof(hints));
		hints.ai_socktype = SOCK_STREAM;
		error = getaddrinfo(host, port, &hints, &res0);
		if (error) {
			fprintf(stderr, "%s: %s\n", host, gai_strerror(error));
			return -1;
		}
		fd = -1;
		for (res = res0; res; res = res->ai_next) {
			error = getnameinfo(res->ai_addr, res->ai_addrlen, hbuf,
			    sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST |
			    NI_NUMERICSERV);
			if (error)
				continue;
			fd = socket(res->ai_family, res->ai_socktype,
			    res->ai_protocol);
			if (fd < 0)
				continue;
			if (bind(fd, res->ai_addr, res->ai_addrlen) < 0) {
				close(fd);
				fd = -1;
				continue;
			}
			break;
		}

		if (fd < 0)
			return luaL_error(L, "connection error");

		if (listen(fd, lua_gettop(L) > 2 ? luaL_checkinteger(L, 3) : 32))
			return luaL_error(L, "listen error");
	}
	data = (int *)lua_newuserdata(L, sizeof(int *));
	*data = fd;
	luaL_getmetatable(L, SOCKET_METATABLE);
	lua_setmetatable(L, -2);

	return 1;
}

static int
luanet_connect(lua_State *L)
{
	struct addrinfo hints, *res, *res0;
	struct sockaddr_un addr;
	int fd, error, *data;
	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
	const char *port, *host;

	host = luaL_checkstring(L, 1);
	if (*host == '/' || *host == '.') {
		fd = socket(AF_UNIX, SOCK_STREAM, 0);

		if (fd >= 0) {
			memset(&addr, 0, sizeof(struct sockaddr_un));
			addr.sun_family = AF_UNIX;
			strncpy(addr.sun_path, host, sizeof(addr.sun_path) - 1);

			if (connect(fd, (struct sockaddr *)&addr,
			    sizeof(struct sockaddr_un)) == -1)
				return luaL_error(L, "connect error");
		}
	} else {
		port = luaL_checkstring(L, 2);

		memset(&hints, 0, sizeof(hints));
		hints.ai_socktype = SOCK_STREAM;
		error = getaddrinfo(host, port, &hints, &res0);
		if (error)
			return luaL_error(L, "%s: %s\n", host,
			    gai_strerror(error));
		fd = -1;
		for (res = res0; res; res = res->ai_next) {
			error = getnameinfo(res->ai_addr, res->ai_addrlen, hbuf,
			    sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST |
			    NI_NUMERICSERV);
			if (error)
				continue;
			fd = socket(res->ai_family, res->ai_socktype,
			    res->ai_protocol);
			if (fd < 0)
				continue;
			if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
				close(fd);
				fd = -1;
				continue;
			}
			break;
		}
	}
	if (fd < 0)
		return luaL_error(L, "connection error");
	else {
		data = (int *)lua_newuserdata(L, sizeof(int *));
		*data = fd;
		luaL_getmetatable(L, SOCKET_METATABLE);
		lua_setmetatable(L, -2);
	}
	return 1;
}

static int
luanet_printf(int fd, const char *fmt, ...)
{
	va_list ap;
	int len, nwritten;
	char *text;

	va_start(ap, fmt);
	len = vasprintf(&text, fmt, ap);
	va_end(ap);
	if (len == -1) {
		fprintf(stderr, "vasprintf failed");
		return -1;
	}

	nwritten = write(fd, text, len);
	free(text);
	return nwritten;
}

static int
luanet_print(lua_State *L)
{
	const char *p;

	p = luaL_checkstring(L, 2);
	if (luanet_printf(*(int *)luaL_checkudata(L, 1, SOCKET_METATABLE),
	    "%s\n", p) < 0)
		return luaL_error(L, "error printing data");
	return 0;
}

static int
luanet_read(lua_State *L)
{
	size_t len, nread;
	int sock;
	int timeout;
	char *buf;

	len = luaL_checkinteger(L, 2);
	sock = *(int *)luaL_checkudata(L, 1, SOCKET_METATABLE);

	if (lua_gettop(L) > 2)
		timeout = luaL_checkinteger(L, 3);
	else
		timeout = -1;

	buf = malloc(len);
	if (buf == NULL)
		lua_pushnil(L);
	else {
		nread = to_read(sock, buf, len, timeout);
		if (nread > 0)
			lua_pushlstring(L, buf, nread);
		else
			lua_pushnil(L);
		free(buf);
	}
	return 1;
}

static int
luanet_readln(lua_State *L)
{
	size_t len;
	int timeout_ms;
	char *buf;

	if (lua_gettop(L) == 2)
		timeout_ms = luaL_checkinteger(L, 2);
	else
		timeout_ms = -1;

	len = asreadln(*(int *)luaL_checkudata(L, 1, SOCKET_METATABLE), &buf,
	    timeout_ms);
	if (len > 0)
		lua_pushstring(L, buf);
	else
		lua_pushnil(L);
	free(buf);
	return 1;
}

static int
luanet_socket(lua_State *L)
{
	lua_pushinteger(L, *(int *)luaL_checkudata(L, 1, SOCKET_METATABLE));
	return 1;
}

static int
luanet_write(lua_State *L)
{
	size_t len;
	const char *p;

	p = luaL_checklstring(L, 2, &len);
	if (write(*(int *)luaL_checkudata(L, 1, SOCKET_METATABLE), p, len)
	    != len)
		return luaL_error(L, "error writing data");
	return 0;
}

static int
luanet_sendfd(lua_State *L)
{
	struct msghdr msg;
	struct cmsghdr *cmsg;
	struct iovec iov[1];
	unsigned char fdbuf[CMSG_SPACE(sizeof(int))];
	char buf[1];
	int fd, passfd;

	/* XXX allow Lua files to be passed as well */
	fd = *(int *)luaL_checkudata(L, 1, SOCKET_METATABLE);
	passfd = *(int *)luaL_checkudata(L, 2, SOCKET_METATABLE);

	buf[0] = 0;
	iov[0].iov_base = buf;
	iov[0].iov_len = 1;

	memset(&msg, 0, sizeof(msg));

	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	msg.msg_control = fdbuf;
	msg.msg_controllen = CMSG_LEN(sizeof(int));

	cmsg = CMSG_FIRSTHDR(&msg);

	cmsg->cmsg_len = CMSG_LEN(sizeof(int));
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;

	*(int *)CMSG_DATA(cmsg) = passfd;

	if (sendmsg(fd, &msg, 0) == -1)
		return luaL_error(L, "sendmsg failed");
	return 0;
}

static int
luanet_recvfd(lua_State *L)
{
	struct msghdr msg;
	struct cmsghdr *cmsg;
	struct iovec iov[1];
	unsigned char fdbuf[CMSG_SPACE(sizeof(int))];
	char buf[16];
	int fd, *data;

	fd = *(int *)luaL_checkudata(L, 1, SOCKET_METATABLE);

	iov[0].iov_base = buf;
	iov[0].iov_len = sizeof(buf);

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	msg.msg_control = fdbuf;
	msg.msg_controllen = CMSG_LEN(sizeof(int));

	cmsg = CMSG_FIRSTHDR(&msg);

	if (recvmsg(fd, &msg, 0) < 0)
		return luaL_error(L, "recvmsg failed");
	data = (int *)lua_newuserdata(L, sizeof(int *));
	*data = *(int *)CMSG_DATA(cmsg);
	luaL_getmetatable(L, SOCKET_METATABLE);
	lua_setmetatable(L, -2);

	return 1;
}

static int
luanet_isvalid(lua_State *L)
{
	int error;
	socklen_t len = sizeof(error);

	lua_pushboolean(L,
	    getsockopt(*(int *)luaL_checkudata(L, 1, SOCKET_METATABLE),
	    SOL_SOCKET, SO_ERROR, &error, &len) == 0 ? 1 : 0);

	return 1;
}

static int
luanet_close(lua_State *L)
{
	close(*(int *)luaL_checkudata(L, 1, SOCKET_METATABLE));
	return 0;
}

static int
luanet_clear(lua_State *L)
{
	int *fd;

	fd = luaL_checkudata(L, 1, SOCKET_METATABLE);
	if (*fd >= 0)  {
		close(*fd);
		*fd = -1;
	}
	return 0;
}

int
luaopen_linux_sys_socket(lua_State *L)
{
	struct luaL_Reg net_methods[] = {
		{ "bind",	luanet_bind },
		{ "connect",	luanet_connect },
		{ NULL, NULL }
	};

	struct luaL_Reg socket_methods[] = {
		{ "accept",	luanet_accept },
		{ "close",	luanet_close },
		{ "print",	luanet_print },
		{ "read",	luanet_read },
		{ "readln",	luanet_readln },
		{ "socket",	luanet_socket },
		{ "write",	luanet_write },
		{ "sendfd",	luanet_sendfd },
		{ "recvfd",	luanet_recvfd },
		{ "isvalid",	luanet_isvalid },
		{ NULL, NULL }
	};

	if (luaL_newmetatable(L, SOCKET_METATABLE)) {
		luaL_setfuncs(L, socket_methods, 0);
		lua_pushliteral(L, "__gc");
		lua_pushcfunction(L, luanet_clear);
		lua_settable(L, -3);

		lua_pushliteral(L, "__close");
		lua_pushcfunction(L, luanet_clear);
		lua_settable(L, -3);

		lua_pushliteral(L, "__index");
		lua_pushvalue(L, -2);
		lua_settable(L, -3);

		lua_pushliteral(L, "__metatable");
		lua_pushliteral(L, "must not access this metatable");
		lua_settable(L, -3);
	}
	lua_pop(L, 1);

	luaL_newlib(L, net_methods);

	return 1;
}
