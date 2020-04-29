
#include <libe/libe.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <microhttpd.h>
#include <b64/cencode.h>
#include <openssl/sha.h>
#include "httpd.h"
#include "opt.h"
#include "slot.h"


static struct MHD_Daemon *httpd_daemon;
static char *httpd_root = NULL;
static pthread_t httpd_thread;
static uint8_t httpd_exec = 0;


static MHD_socket httpd_slot_sockets[SLOT_COUNT];


static int httpd_websocket_send(int slot_id, void *data, size_t size, uint8_t opcode)
{
	if (slot_id < 0 || slot_id >= SLOT_COUNT || httpd_slot_sockets[slot_id] < 0) {
		return -1;
	}

	uint8_t header[2] = { opcode, 0 };
	uint8_t *p = data;
	for (; size > 0; ) {
		/* set fin-bit if last of data */
		header[0] |= size < 126 ? 0x80 : 0x00;
		/* send at most 125 bytes at once */
		header[1] = size < 126 ? size : 125;
		/* send header and then data */
		send(httpd_slot_sockets[slot_id], header, 2, 0);
		send(httpd_slot_sockets[slot_id], p, size, 0);
		/* next block */
		size -= header[1];
		p += header[1];
		/* zero opcode to say that next frame continues the one before */
		header[0] = 0x00;
	}

	return 0;
}

static int httpd_404(struct MHD_Connection *connection, int code)
{
	int err;
	struct MHD_Response *response;
	static char *data = "<html><head><title>404</title></head><body><h1>404</h1></body></html>";
	response = MHD_create_response_from_buffer(strlen(data),
	           (void *)data, MHD_RESPMEM_PERSISTENT);
	MHD_add_response_header(response, "Content-Type", "text/html; charset=utf-8");
	err = MHD_queue_response(connection, code, response);
	MHD_destroy_response(response);
	return err;
}

static int httpd_get_default(struct MHD_Connection *connection, const char *url)
{
	int err = 0, n;
	char *data = NULL;
	struct MHD_Response *response;
	char *file = NULL;
	FILE *f = NULL;
	const char *ext = NULL;

	if (strlen(url) == 1) {
		url = "/index.html";
	}
	ext = url + strlen(url) - 1;
	for (; ext > url && *ext != '.' ; ext--);
	ext++;

	/* check filesystem */
	if (asprintf(&file, "%s%s", httpd_root, url) < 0) {
		return httpd_404(connection, MHD_HTTP_NOT_FOUND);
	}
	f = fopen(file, "r");
	free(file);
	if (!f) {
		return httpd_404(connection, MHD_HTTP_NOT_FOUND);
	}

	/* read file size */
	fseek(f, 0, SEEK_END);
	n = ftell(f);
	fseek(f, 0, SEEK_SET);

	/* allocate buffer for reading file and set to zero (fread() can fail and we have zeroed buffer)*/
	data = malloc(n);
	memset(data, 0, n);
	n = fread(data, 1, n, f);
	fclose(f);

	response = MHD_create_response_from_buffer(n,
	           (void *)data, MHD_RESPMEM_MUST_FREE);
	if (strcmp(ext, "css") == 0) {
		MHD_add_response_header(response, "Content-Type", "text/css; charset=utf-8");
	} else 	if (strcmp(ext, "js") == 0) {
		MHD_add_response_header(response, "Content-Type", "application/javascript; charset=utf-8");
	} else 	if (strcmp(ext, "json") == 0) {
		MHD_add_response_header(response, "Content-Type", "application/json; charset=utf-8");
	} else {
		MHD_add_response_header(response, "Content-Type", "text/html; charset=utf-8");
	}
	err = MHD_queue_response(connection, MHD_HTTP_OK, response);
	MHD_destroy_response(response);

	return err;
}

static void httpd_websocket_handler(void *cls,
                                    struct MHD_Connection *connection,
                                    void *con_cls,
                                    const char *extra_in,
                                    size_t extra_in_size,
                                    MHD_socket sock,
                                    struct MHD_UpgradeResponseHandle *urh)
{
	httpd_slot_sockets[0] = sock;
}


// if (slot->websocket >= 0) {
// }



static int httpd_upgrade_websocket(struct MHD_Connection *connection)
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
	response = MHD_create_response_for_upgrade(httpd_websocket_handler, NULL);
	MHD_add_response_header(response, "Upgrade", "websocket");
	MHD_add_response_header(response, "Connection", "Upgrade");
	MHD_add_response_header(response, "Sec-WebSocket-Accept", s_hash);
	err = MHD_queue_response(connection, MHD_HTTP_SWITCHING_PROTOCOLS, response);
	MHD_destroy_response(response);

	return err;
}

static void *httpd_thread_func(void *data)
{
	while (httpd_exec) {
		uint8_t data[1024];
		struct timeval tv;
		fd_set r_fds, w_fds, e_fds;
		int max_fd = 0;
		int n;

		/* clear all fd sets */
		FD_ZERO(&r_fds);
		FD_ZERO(&w_fds);
		FD_ZERO(&e_fds);

		/* add http daemon fd sets */
		MHD_get_fdset(httpd_daemon, &r_fds, &w_fds, &e_fds, &max_fd);

		/* add tx fd sets */
		for (int id = 0; id < SLOT_COUNT; id++) {
			int fd = slot_fd_tx(id);
			FD_SET(fd, &r_fds);
			max_fd = max_fd < fd ? fd : max_fd;
		}

		for (int id = 0; id < SLOT_COUNT; id++) {
			if (httpd_slot_sockets[id] >= 0) {
				FD_SET(httpd_slot_sockets[id], &r_fds);
				max_fd = max_fd < httpd_slot_sockets[id] ? httpd_slot_sockets[id] : max_fd;
			}
		}

		/* 10 ms timeout */
		tv.tv_sec = 0;
		tv.tv_usec = 10000;

		/* wait */
		n = select(max_fd + 1, &r_fds, &w_fds, &e_fds, &tv);
		if (n < 0) {
			CRIT_MSG("select() failed, reason: %s", strerror(errno));
			break;
		} else if (n == 0) {
			/* timeout */
			continue;
		}

		/* check incoming data from websockets */
		for (int id = 0; id < SLOT_COUNT; id++) {
			if (httpd_slot_sockets[id] >= 0 && FD_ISSET(httpd_slot_sockets[id], &r_fds)) {
				n = recv(httpd_slot_sockets[id], data, sizeof(data), 0);
				if (n <= 0) {
					/* connection closed */
					// httpd_slot_sockets[id] = -1;
					DEBUG_MSG("closed websocket for slot #%d", id);
				} else if ((data[0] & 0x0f) == 0x08) {
						shutdown(httpd_slot_sockets[id], SHUT_RDWR);
						httpd_slot_sockets[id] = -1;
						DEBUG_MSG("closed websocket connection to slot #%d", id);
				} else {
					/* send data to terminal ... */
					for (int i = 0; i < n; i++) {
						printf("%02x ", data[i]);
					}
					printf("\n");
				}
			}
			if (httpd_slot_sockets[id] >= 0 && FD_ISSET(httpd_slot_sockets[id], &e_fds)) {
				DEBUG_MSG("exp");
			}
		}

		MHD_run_from_select(httpd_daemon, &r_fds, &w_fds, &e_fds);

		/* check tx fd sets */
		for (int id = 0; id < SLOT_COUNT; id++) {
			int fd = slot_fd_tx(id);
			if (FD_ISSET(fd, &r_fds)) {
				n = read(fd, data, sizeof(data));
				for (int j = 0; j < n; j++) {
					if (isprint(data[j]) || iscntrl(data[j])) {
						printf("%c", data[j]);
					} else if (data[j] != 0) {
						printf("{%02x}", data[j]);
					}
				}
				httpd_websocket_send(id, data, n, 0x02);
			}
		}
	}

	DEBUG_MSG("http daemon thread exit");
	return NULL;
}

static int httpd_request_handler(void *cls, struct MHD_Connection *connection,
                                 const char *url,
                                 const char *method, const char *version,
                                 const char *upload_data,
                                 size_t *upload_data_size, void **con_cls)
{
	DEBUG_MSG("request %s to url %s", method, url);

	if (strcmp(method, "GET") != 0) {
		return httpd_404(connection, MHD_HTTP_NOT_FOUND);
	}

	const char *header;

	header = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Upgrade");
	if (header && strcmp(header, "websocket") == 0) {
		return httpd_upgrade_websocket(connection);
	}

	// if (strcmp(url, "/set") == 0) {
	// 	return httpd_set(connection);
	// }
	// if (strcmp(url, "/get") == 0) {
	// 	return httpd_get(connection);
	// }
	return httpd_get_default(connection, url);
}

int httpd_init(char *address, uint16_t port, char *root)
{
	httpd_daemon = MHD_start_daemon(MHD_USE_ERROR_LOG | MHD_ALLOW_UPGRADE,
	                                port, NULL, NULL,
	                                &httpd_request_handler, NULL, MHD_OPTION_END);
	CRIT_IF_R(!httpd_daemon, -1, "unable to initialize development device front end http daemon");
	NOTICE_MSG("listening to http requests at http://%s:%d", address ? address : "0.0.0.0", port);
	httpd_root = strdup(root);

	/* start handler thread */
	httpd_exec = 1;
	pthread_create(&httpd_thread, NULL, httpd_thread_func, NULL);

	return 0;
}

void httpd_quit(void)
{
	httpd_exec = 0;
	pthread_join(httpd_thread, NULL);

	if (httpd_daemon) {
		MHD_stop_daemon(httpd_daemon);
		httpd_daemon = NULL;
	}
	if (httpd_root) {
		free(httpd_root);
		httpd_root = NULL;
	}
}
