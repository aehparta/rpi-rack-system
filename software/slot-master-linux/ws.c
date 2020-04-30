/*
 * Websockets.
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <b64/cencode.h>
#include <openssl/sha.h>
#include "httpd.h"
#include "ws.h"


struct ws_data {
	size_t hsize;
	uint8_t hdata[2 + 8 + 4];
	size_t hdata_c;

	uint64_t size;
	uint8_t *data;
	size_t data_c;
};

struct ws_handle {
	MHD_socket fd;
	char *url;

	struct ws_data data;

	/* for closing the connection properly */
	struct MHD_UpgradeResponseHandle *urh;
};

static int ws_fd = -1;
static pthread_t ws_t;
static int ws_exec = 0;


/* internal function definitions */
static void *ws_thread_func(void *userdata);
static void ws_upgrade_ready(void *cls, struct MHD_Connection *connection, void *con_cls, const char *extra_in, size_t extra_in_size, MHD_socket fd, struct MHD_UpgradeResponseHandle *urh);


int ws_init(void)
{
	ws_fd = epoll_create1(0);
	if (ws_fd < 0) {
		return -1;
	}
	return 0;
}

void ws_quit(void)
{
	if (ws_exec) {
		ws_exec = 0;
		pthread_join(ws_t, NULL);
	}
	if (ws_fd >= 0) {
		close(ws_fd);
		ws_fd = -1;
	}
}

int ws_start(void)
{
	ws_exec = 1;
	pthread_create(&ws_t, NULL, ws_thread_func, NULL);
	return 0;
}

int ws_register_url(char *url_pattern, void *userdata)
{
	return httpd_register_url_with_type(HTTPD_URL_REGEX_TYPE_WEBSOCKET, "GET", url_pattern, NULL, userdata);
}

int ws_send(int fd, void *data, size_t size, uint8_t opcode)
{
	uint8_t header[2] = { opcode, 0 };
	uint8_t *p = data;
	for (; size > 0; ) {
		/* set fin-bit if last of data */
		header[0] |= size < 126 ? 0x80 : 0x00;
		/* send at most 125 bytes at once */
		header[1] = size < 126 ? size : 125;
		/* send header and then data */
		send(fd, header, 2, 0);
		send(fd, p, size, 0);
		/* next block */
		size -= header[1];
		p += header[1];
		/* zero opcode to say that next frame continues the one before */
		header[0] = 0x00;
	}

	return 0;
}

int ws_upgrade_init(const char *url, struct MHD_Connection *connection)
{
	int err = 0, n;
	struct MHD_Response *response;
	const char *header;
	char s_hash[32];
	uint8_t hash[SHA_DIGEST_LENGTH];
	const char *append = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	base64_encodestate b64estate;
	SHA_CTX shac;

	/* get key */
	header = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Sec-WebSocket-Key");
	if (!header) {
		return MHD_NO;
	}

	/* hash the key */
	SHA1_Init(&shac);
	SHA1_Update(&shac, (unsigned char *)header, strlen(header));
	SHA1_Update(&shac, (unsigned char *)append, strlen(append));
	SHA1_Final(hash, &shac);

	/* encode */
	base64_init_encodestate(&b64estate);
	n = base64_encode_block((char *)hash, SHA_DIGEST_LENGTH, s_hash, &b64estate);
	n += base64_encode_blockend(s_hash + n, &b64estate);
	s_hash[n] = '\0';
	s_hash[n - 1] = isspace(s_hash[n - 1]) ? '\0' : s_hash[n - 1]; /* bug in base64 lib which appends line break? */

	/* create response */
	response = MHD_create_response_for_upgrade(ws_upgrade_ready, strdup(url));
	MHD_add_response_header(response, "Upgrade", "websocket");
	MHD_add_response_header(response, "Connection", "Upgrade");
	MHD_add_response_header(response, "Sec-WebSocket-Accept", s_hash);
	err = MHD_queue_response(connection, MHD_HTTP_SWITCHING_PROTOCOLS, response);
	MHD_destroy_response(response);

	return err;
}


/* internals */


static void ws_free(struct ws_handle *wsh)
{
	if (wsh->data.data) {
		free(wsh->data.data);
	}
	if (wsh->url) {
		free(wsh->url);
	}
	free(wsh);
}

static void ws_upgrade_ready(void *cls,
                      struct MHD_Connection *connection,
                      void *con_cls,
                      const char *extra_in,
                      size_t extra_in_size,
                      MHD_socket fd,
                      struct MHD_UpgradeResponseHandle *urh)
{
	struct epoll_event e;
	struct ws_handle *wsh = malloc(sizeof(*wsh));

	memset(wsh, 0, sizeof(*wsh));
	wsh->fd = fd;
	wsh->url = strdup(cls);
	wsh->urh = urh;

	memset(&e, 0, sizeof(e));
	e.events = EPOLLIN;
	e.data.ptr = wsh;
	if (epoll_ctl(ws_fd, EPOLL_CTL_ADD, fd, &e) < 0) {
		fprintf(stderr, "epoll_ctl() failed to add new socket, reason: %s\n", strerror(errno));
	}

	printf("upraded to websocket, url to which the call was made: %s\n", wsh->url);
}

static void ws_event_handler(struct ws_handle *wsh)
{
	int fd = wsh->fd;
	struct ws_data *wsd = &wsh->data;
	uint8_t peek;
	int n;

	/* peek check if connection is closed */
	n = recv(fd, &peek, 1, MSG_PEEK);
	if (n < 1 || (wsd->hsize == 0 && (peek & 0x0f) == 0x08)) {
		/* connection closed */
		epoll_ctl(ws_fd, EPOLL_CTL_DEL, fd, NULL);
		MHD_upgrade_action(wsh->urh, MHD_UPGRADE_ACTION_CLOSE);
		ws_free(wsh);
		return;
	}

	/* if base header should be read */
	if (wsd->hsize < 2) {
		wsd->hsize += read(fd, wsd->hdata + wsd->hsize, 2 - wsd->hsize);
		if (wsd->hsize < 2) {
			return;
		}
		wsd->hdata_c = 2;

		// printf("base header received, fin: %d, opcode: %02x, mask used: %d, len: %d\n", wsd->hdata[0] >> 7, wsd->hdata[0] & 0xf, wsd->hdata[1] >> 7, wsd->hdata[1] & 0x7f);

		/* 2/8 bytes for extended payload length */
		if ((wsd->hdata[1] & 0x7f) == 126) {
			wsd->hsize += 2;
		} else if ((wsd->hdata[1] & 0x7f) == 127) {
			wsd->hsize += 8;
		} else {
			/* we can already calculate final data size */
			wsd->size = wsd->hdata[1] & 0x7f;
			// printf("tiny data size: %lld\n", wsd->size);
		}
		/* add 4 bytes if mask should be read */
		wsd->hsize += (wsd->hdata[1] & 0x80 ? 4 : 0);

		/* return if no more data */
		n = recv(fd, &peek, 1, MSG_PEEK | MSG_DONTWAIT);
		if (n < 1) {
			return;
		}
	}

	/* read more of the header if needed */
	if (wsd->hsize > wsd->hdata_c) {
		wsd->hdata_c += read(fd, wsd->hdata + wsd->hdata_c, wsd->hsize - wsd->hdata_c);
		if (wsd->hsize > wsd->hdata_c) {
			return;
		}

		/* calculate final data size */
		if ((wsd->hdata[1] & 0x7f) == 126) {
			wsd->size = wsd->hdata[2] << 8 | wsd->hdata[3];
			// printf("short data size: %lld\n", wsd->size);
		} else if ((wsd->hdata[1] & 0x7f) == 127) {
			for (int i = 0; i < 8; i++) {
				wsd->size |= wsd->hdata[2 + i] << (8 * (7 - i));
			}
			// printf("long data size: %lld\n", wsd->size);
		}

		/* return if no more data */
		n = recv(fd, &peek, 1, MSG_PEEK | MSG_DONTWAIT);
		if (n < 1) {
			return;
		}
	}

	/* allocate data buffer if not done yet */
	if (!wsd->data) {
		wsd->data = malloc(wsd->size);
	}

	/* read incoming data */
	wsd->data_c += read(fd, wsd->data + wsd->data_c, wsd->size - wsd->data_c);

	/* if not full packet yet */
	if (wsd->size > wsd->data_c) {
		return;
	}

	/* unmask */
	if (wsd->hdata[1] & 0x80) {
		int moff = 2;
		if ((wsd->hdata[1] & 0x7f) == 126) {
			moff += 2;
		} else if ((wsd->hdata[1] & 0x7f) == 127) {
			moff += 8;
		}
		for (int i = 0; i < wsd->size; i++) {
			wsd->data[i] ^= wsd->hdata[moff + (i % 4)];
		}
	}

	for (int i = 0; i < wsd->size; i++) {
		printf("%c", wsd->data[i]);
	}
	fflush(stdout);

	free(wsd->data);
	memset(wsd, 0, sizeof(*wsd));

	// printf("got data %d bytes:", n);
	// for (int i = 0; i < n; i++) {
	// 	printf(" %02x", data[i]);
	// }
	// printf("\n");
}

static void *ws_thread_func(void *userdata)
{
	while (ws_exec) {
		/* get single event in one wait, not really in hurry here */
		struct epoll_event e;
		int n = epoll_wait(ws_fd, &e, 1, 100);
		if (n < 1) {
			continue;
		}
		ws_event_handler(e.data.ptr);
	}
	return NULL;
}
