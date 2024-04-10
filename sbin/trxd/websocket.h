/*
 * Copyright (c) 2014 - 2024 Micro Systems Marc Balmer, CH-5073 Gipf-Oberfrick.
 * Copyright (c) 2014 Putilov Andrey
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

#ifndef __WEBSOCKET_H__
#define __WEBSOCKET_H__

#include <stdint.h>

static const char connectionField[] = "Connection: ";
static const char upgrade[] = "upgrade";
static const char upgrade2[] = "Upgrade";
static const char upgradeField[] = "Upgrade: ";
static const char websocket[] = "websocket";
static const char hostField[] = "Host: ";
static const char originField[] = "Origin: ";
static const char keyField[] = "Sec-WebSocket-Key: ";
static const char protocolField[] = "Sec-WebSocket-Protocol: ";
static const char versionField[] = "Sec-WebSocket-Version: ";
static const char version[] = "13";
static const char secret[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

enum wsFrameType {
	/* errors starting from 0xF0 */
	WS_EMPTY_FRAME = 0xf0,
	WS_ERROR_FRAME = 0xf1,
	WS_INCOMPLETE_FRAME = 0xf2,
	WS_TEXT_FRAME = 0x01,
	WS_BINARY_FRAME = 0x02,
	WS_PING_FRAME = 0x09,
	WS_PONG_FRAME = 0x0a,
	WS_OPENING_FRAME = 0xf3,
	WS_CLOSING_FRAME = 0x08
};

enum wsState {
	WS_STATE_OPENING,
	WS_STATE_NORMAL,
	WS_STATE_CLOSING
};

struct handshake {
	char		*host;
	char		*origin;
	char		*key;
	char		*resource;
	enum wsFrameType frameType;
};

extern enum wsFrameType wsParseHandshake(const uint8_t *, size_t,
    struct handshake *);

extern void wsGetHandshakeAnswer(const struct handshake *, uint8_t *,
    size_t *);

extern void wsMakeFrame(const uint8_t *, size_t, uint8_t *, size_t *,
    enum wsFrameType);

extern size_t wsGetPayloadLength(const uint8_t *, size_t, uint8_t *,
    enum wsFrameType *);

extern enum wsFrameType wsParseInputFrame(uint8_t *, size_t, uint8_t **,
    size_t *);

extern enum wsFrameType wsRead(char **dest, size_t *,
    int(*readfunc)(void *, unsigned char *, size_t),
    int(*writefunc)(void *, unsigned char *, size_t), void *);

extern void nullHandshake(struct handshake *);
extern void freeHandshake(struct handshake *);

#endif  /* __WEBSOCKET_H__ */
