/*
 * Copyright (C) 2014 - 2024 Micro Systems Marc Balmer, CH-5073 Gipf-Oberfrick
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

#include <openssl/opensslconf.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>

#include <assert.h>
#include <endian.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "base64.h"
#include "websocket.h"

#define INITIAL_BUFSIZE		256

void
nullHandshake(struct handshake *hs)
{
	hs->host = NULL;
	hs->origin = NULL;
	hs->resource = NULL;
	hs->key = NULL;
	hs->frameType = WS_EMPTY_FRAME;
}

void
freeHandshake(struct handshake *hs)
{
	free(hs->host);
	free(hs->origin);
	free(hs->resource);
	free(hs->key);
	nullHandshake(hs);
}

static char *
getUptoLinefeed(const char *startFrom)
{
	char *writeTo = NULL;
	uint8_t newLength = strstr(startFrom, "\r\n") - startFrom;

	assert(newLength);
	writeTo = (char *)malloc(newLength + 1);
	assert(writeTo);
	memcpy(writeTo, startFrom, newLength);
	writeTo[newLength] = 0;

	return writeTo;
}

enum wsFrameType
wsParseHandshake(const uint8_t *inputFrame, size_t inputLength,
    struct handshake *hs)
{
	const char *inputPtr = (const char *)inputFrame;
	const char *endPtr = (const char *)inputFrame + inputLength;
	int connectionFlag = 0;
	int upgradeFlag = 0;
	int subprotocolFlag = 0;
	int versionMismatch = 0;

	if (!strstr((const char *)inputFrame, "\r\n\r\n"))
		return WS_INCOMPLETE_FRAME;

	if (memcmp(inputFrame, "GET ", 4) != 0)
		return WS_ERROR_FRAME;

	/* measure resource size */
	char *first = strchr((const char *)inputFrame, ' ');
	if (!first)
		return WS_ERROR_FRAME;
	first++;
	char *second = strchr(first, ' ');
	if (!second)
		return WS_ERROR_FRAME;

	if (hs->resource) {
		free(hs->resource);
		hs->resource = NULL;
	}
	hs->resource = (char *)malloc(second - first + 1);
	assert(hs->resource);

	if (sscanf(inputPtr, "GET %s HTTP/1.1\r\n", hs->resource) != 1)
		return WS_ERROR_FRAME;
	inputPtr = strstr(inputPtr, "\r\n") + 2;

	/* parse next lines */
	while (inputPtr < endPtr && inputPtr[0] != '\r'
	    && inputPtr[1] != '\n') {
		if (!strncasecmp(inputPtr, hostField, strlen(hostField))) {
			inputPtr += strlen(hostField);
			free(hs->host);
			hs->host = getUptoLinefeed(inputPtr);
		} else if (!strncasecmp(inputPtr, originField,
		    strlen(originField))) {
			inputPtr += strlen(originField);
			free(hs->origin);
			hs->origin = getUptoLinefeed(inputPtr);
		} else if (!strncasecmp(inputPtr, protocolField,
		    strlen(protocolField))) {
			inputPtr += strlen(protocolField);
			subprotocolFlag = 1;
		} else if (!strncasecmp(inputPtr, keyField, strlen(keyField))) {
			inputPtr += strlen(keyField);
			free(hs->key);
			hs->key = getUptoLinefeed(inputPtr);
		} else if (!strncasecmp(inputPtr, versionField,
		    strlen(versionField))) {
			char *versionString;

			inputPtr += strlen(versionField);
			versionString = getUptoLinefeed(inputPtr);
			if (memcmp(versionString, version, strlen(version)))
				versionMismatch = 1;
			free(versionString);
		} else if (!strncasecmp(inputPtr, connectionField,
		    strlen(connectionField))) {
			char *connectionValue;

			inputPtr += strlen(connectionField);
			connectionValue = getUptoLinefeed(inputPtr);
			assert(connectionValue);
			if (strcasestr(connectionValue, upgrade) != NULL)
				connectionFlag = 1;
			free(connectionValue);
		} else if (!strncasecmp(inputPtr, upgradeField,
		    strlen(upgradeField))) {
			char *compare;

			inputPtr += strlen(upgradeField);
			compare = getUptoLinefeed(inputPtr);
			assert(compare);
			if (!strncasecmp(compare, websocket, strlen(websocket)))
				upgradeFlag = 1;
			free(compare);
		}
		inputPtr = strstr(inputPtr, "\r\n") + 2;
	}

	/* we have read all data, so check them */
	if (!hs->host || !hs->key || !connectionFlag || !upgradeFlag ||
	    subprotocolFlag || versionMismatch)
		hs->frameType = WS_ERROR_FRAME;
	else
		hs->frameType = WS_OPENING_FRAME;

	return hs->frameType;
}

void
wsGetHandshakeAnswer(const struct handshake *hs, uint8_t *outFrame,
    size_t *outLength)
{
	BIO *bio, *md;
	char *responseKey, *b64;
	unsigned char mdbuf[EVP_MAX_MD_SIZE];
	int mdlen;

	assert(outFrame && *outLength);
	assert(hs->frameType == WS_OPENING_FRAME);
	assert(hs && hs->key);

	uint8_t length = strlen(hs->key) + strlen(secret);
	responseKey = malloc(length + 1);
	memcpy(responseKey, hs->key, strlen(hs->key));
	memcpy(&(responseKey[strlen(hs->key)]), secret, strlen(secret));
	responseKey[length] = '\0';

	/* Setup the message digest BIO chain */
	bio = BIO_new(BIO_s_null());
	md = BIO_new(BIO_f_md());
	BIO_set_md(md, EVP_sha1());
	bio = BIO_push(md, bio);
	BIO_printf(md, "%s", responseKey);

	mdlen = BIO_gets(md, (char *)mdbuf, EVP_MAX_MD_SIZE);
 	b64 = base64(mdbuf, mdlen);
	BIO_free(bio);

	size_t written = sprintf((char *)outFrame,
	    "HTTP/1.1 101 Switching Protocols\r\n"
	    "%s%s\r\n"
	    "%s%s\r\n"
	    "Sec-WebSocket-Accept: %s\r\n\r\n", upgradeField,
	     websocket, connectionField, upgrade2, b64);
	free(b64);

	/* if the assert fails, that means, that we corrupt memory */
	assert(written <= *outLength);
	*outLength = written;
}

void
wsMakeFrame(const uint8_t *data, size_t dataLength, uint8_t *outFrame,
    size_t *outLength, enum wsFrameType frameType)
{
	assert(outFrame && outLength);
	assert(frameType < 0x10);
	if (dataLength > 0)
		assert(data);

	outFrame[0] = 0x80 | frameType;

	if (dataLength <= 125) {
		outFrame[1] = dataLength;
		*outLength = 2;
	} else if (dataLength <= 0xFFFF) {
		outFrame[1] = 126;
		uint16_t payloadLength16b = htons(dataLength);
		memcpy(&outFrame[2], &payloadLength16b, 2);
		*outLength = 4;
	} else {
		outFrame[1] = 127;
		memcpy(&outFrame[2], &dataLength, 8);
		*outLength = 10;
	}
	memcpy(&outFrame[*outLength], data, dataLength);
	*outLength += dataLength;
}

size_t
wsGetPayloadLength(const uint8_t *inputFrame, size_t inputLength,
    uint8_t *payloadFieldExtraBytes, enum wsFrameType *frameType)
{
	size_t payloadLength = inputFrame[1] & 0x7F;

	*payloadFieldExtraBytes = 0;
	if ((payloadLength == 0x7E && inputLength < 4) ||
	    (payloadLength == 0x7F && inputLength < 10)) {
		*frameType = WS_INCOMPLETE_FRAME;
		return 0;
	}
	if (payloadLength == 0x7F && (inputFrame[3] & 0x80) != 0x0) {
		*frameType = WS_ERROR_FRAME;
		return 0;
	}

	if (payloadLength == 0x7E) {
		*payloadFieldExtraBytes = 2;

		payloadLength = be16toh(*(uint16_t *)&inputFrame[2]);
	} else if (payloadLength == 0x7F) {
		*payloadFieldExtraBytes = 8;

		if (payloadLength > SIZE_MAX) {
			*frameType = WS_ERROR_FRAME;
			return 0;
		}
	}
	return payloadLength;
}

enum wsFrameType
wsParseInputFrame(uint8_t *inputFrame, size_t inputLength, uint8_t **dataPtr,
    size_t *dataLength)
{
	assert(inputFrame && inputLength);

	uint8_t opcode = inputFrame[0] & 0x0F;
	if (opcode == WS_TEXT_FRAME ||
	    opcode == WS_BINARY_FRAME ||
	    opcode == WS_CLOSING_FRAME ||
	    opcode == WS_PING_FRAME ||
	    opcode == WS_PONG_FRAME) {
		enum wsFrameType frameType = opcode;

		uint8_t payloadFieldExtraBytes = 0;
		size_t payloadLength = wsGetPayloadLength(inputFrame,
		    inputLength, &payloadFieldExtraBytes, &frameType);
		if (payloadLength > 0) {
			size_t i;
			uint8_t *maskingKey = &inputFrame[2 +
			     payloadFieldExtraBytes];

			assert(payloadLength == inputLength - 6 -
			    payloadFieldExtraBytes);

			*dataPtr = &inputFrame[2 + payloadFieldExtraBytes + 4];
			*dataLength = payloadLength;

			for (i = 0; i < *dataLength; i++)
				(*dataPtr)[i] = (*dataPtr)[i] ^ maskingKey[i%4];
		}
		return opcode;
	}
	return WS_ERROR_FRAME;
}

enum wsFrameType
wsRead(char **dest, size_t *destlen,
    int(*readfunc)(void *, unsigned char *, size_t),
    int(*writefunc)(void *, unsigned char *, size_t), void *client_data)
{
	unsigned char *data;
	char *buf;
	size_t bufsize, nread, len, datasize;
	int type;
	uint8_t payloadFieldExtraBytes = 0;
	size_t payloadLength;
	enum wsFrameType frameType;

	bufsize = INITIAL_BUFSIZE;
	buf = malloc(bufsize);
	if (buf == NULL)
		return -1;

	nread = len = 0;
	type = WS_INCOMPLETE_FRAME;
	do {
		/*
		 * The most common frame header is six bytes long, try to read
		 * 6 bytes and then determine the actual payload size and read
		 * the remaining data.
		 */
		do {
			nread = readfunc(client_data, buf, 6);
			if (nread <= 0) {	/* remote closed */
				free(buf);
				return -1;
			}
			len += nread;
		} while (len < 2);	/* 2 is the minimum */

		if (((buf[0] & 0x70) != 0x0) || ((buf[0] & 0x80) != 0x80) ||
		    ((buf[1] & 0x80) != 0x80)) {
			free(buf);
			return -1;
		}

		payloadLength = wsGetPayloadLength(buf, len,
		    &payloadFieldExtraBytes, &frameType);

		/* Ensure buf can hold the complete payload */
		if (6 + payloadFieldExtraBytes + payloadLength > bufsize) {
			bufsize = 6 + payloadFieldExtraBytes + payloadLength;
			buf = realloc(buf, bufsize);
			if (buf == NULL)
				return -1;
		}

		do {
			nread = readfunc(client_data, buf + len, payloadLength +
			    payloadFieldExtraBytes);
			if (nread <= 0) {
				free(buf);
				return -1;
			}
			len += nread;
		} while (len < 6 + payloadFieldExtraBytes + payloadLength);

		type = wsParseInputFrame((unsigned char *)buf, len, &data,
			&datasize);

		switch (type) {
		case WS_CLOSING_FRAME:
			wsMakeFrame(NULL, 0, (unsigned char *)buf, &datasize,
			    WS_CLOSING_FRAME);
			writefunc(client_data, buf, datasize);
			free(buf);
			return -1;
		case WS_PING_FRAME:
			data[datasize] = '\0';
			wsMakeFrame(data, datasize, (unsigned char *)buf, &datasize,
			    WS_PONG_FRAME);
			writefunc(client_data, buf, datasize);
			len = 0;
			type = WS_INCOMPLETE_FRAME;
			break;
		case WS_TEXT_FRAME:
			data[datasize] = '\0';
			*dest = strdup(data);
			if (destlen != NULL)
				*destlen = datasize;
			if (*dest == NULL) {
				free(buf);
				return -1;
			}
			break;
		case WS_INCOMPLETE_FRAME:
			break;
		default:
			free(buf);
			return -1;
		}
	} while (type == WS_INCOMPLETE_FRAME);
	free(buf);
	return 0;
}
