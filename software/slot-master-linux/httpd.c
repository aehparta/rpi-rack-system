
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <b64/cencode.h>
#include <openssl/sha.h>
#include "linkedlist.h"
#include "httpd.h"


#define URL_MAX_REGEX_SUBSTRINGS 30 /* should be multiple of 3 */


struct httpd_websocket {
	MHD_socket sock;
	const char *url;

	/* linkedlist handles */
	struct httpd_websocket *prev;
	struct httpd_websocket *next;
};

struct httpd_url_regex {
	pcre *mrex;
	pcre *urex;

	int (*callback)(struct MHD_Connection *connection, const char *url, const char *method, const char *upload_data, size_t *upload_data_size, const char **substrings, size_t substrings_c, void *userdata);
	void *userdata;

	/* websocket related stuff */
	struct httpd_websocket *ws_first;
	struct httpd_websocket *ws_last;

	/* linkedlist handles */
	struct httpd_url_regex *prev;
	struct httpd_url_regex *next;
};


static struct MHD_Daemon *daemon_handle;
static char *www_root = NULL;
static struct httpd_url_regex *url_first = NULL;
static struct httpd_url_regex *url_last = NULL;


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
	int err = 0;
	struct MHD_Response *response;
	char *file = NULL;
	int fd = -1;
	const char *ext = NULL;
	struct stat st;

	if (strlen(url) == 1) {
		url = "/index.html";
	}
	ext = url + strlen(url) - 1;
	for (; ext > url && *ext != '.' ; ext--);
	ext++;

	/* check filesystem */
	if (asprintf(&file, "%s%s", www_root, url) < 0) {
		return httpd_404(connection, MHD_HTTP_NOT_FOUND);
	}
	fd = open(file, O_RDONLY);
	free(file);
	if (fd < 0) {
		return httpd_404(connection, MHD_HTTP_NOT_FOUND);
	}

	/* read file size */
	fstat(fd, &st);

	response = MHD_create_response_from_fd(st.st_size, fd);
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
	printf("upraded socket, url: %s\n", (char *)cls);
	// httpd_slot_sockets[0] = sock;
}

static int httpd_upgrade_websocket(const char *url, struct MHD_Connection *connection)
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
	response = MHD_create_response_for_upgrade(httpd_websocket_handler, strdup(url));
	MHD_add_response_header(response, "Upgrade", "websocket");
	MHD_add_response_header(response, "Connection", "Upgrade");
	MHD_add_response_header(response, "Sec-WebSocket-Accept", s_hash);
	err = MHD_queue_response(connection, MHD_HTTP_SWITCHING_PROTOCOLS, response);
	MHD_destroy_response(response);

	return err;
}

// static void *httpd_thread_func(void *data)
// {
// 	while (httpd_exec) {
// 		uint8_t data[1024];
// 		struct timeval tv;
// 		fd_set r_fds, w_fds, e_fds;
// 		int max_fd = 0;
// 		int n;

// 		/* clear all fd sets */
// 		FD_ZERO(&r_fds);
// 		FD_ZERO(&w_fds);
// 		FD_ZERO(&e_fds);

// 		/* add http daemon fd sets */
// 		MHD_get_fdset(daemon_handle, &r_fds, &w_fds, &e_fds, &max_fd);

// 		/* add tx fd sets */
// 		for (int id = 0; id < SLOT_COUNT; id++) {
// 			int fd = slot_fd_tx(id);
// 			FD_SET(fd, &r_fds);
// 			max_fd = max_fd < fd ? fd : max_fd;
// 		}

// 		for (int id = 0; id < SLOT_COUNT; id++) {
// 			if (httpd_slot_sockets[id] >= 0) {
// 				FD_SET(httpd_slot_sockets[id], &r_fds);
// 				max_fd = max_fd < httpd_slot_sockets[id] ? httpd_slot_sockets[id] : max_fd;
// 			}
// 		}

// 		/* 10 ms timeout */
// 		tv.tv_sec = 0;
// 		tv.tv_usec = 10000;

// 		/* wait */
// 		n = select(max_fd + 1, &r_fds, &w_fds, &e_fds, &tv);
// 		if (n < 0) {
// 			CRIT_MSG("select() failed, reason: %s", strerror(errno));
// 			break;
// 		} else if (n == 0) {
// 			/* timeout */
// 			continue;
// 		}

// 		/* check incoming data from websockets */
// 		for (int id = 0; id < SLOT_COUNT; id++) {
// 			if (httpd_slot_sockets[id] >= 0 && FD_ISSET(httpd_slot_sockets[id], &r_fds)) {
// 				n = recv(httpd_slot_sockets[id], data, sizeof(data), 0);
// 				if (n <= 0) {
// 					/* connection closed */
// 					// httpd_slot_sockets[id] = -1;
// 					DEBUG_MSG("closed websocket for slot #%d", id);
// 				} else if ((data[0] & 0x0f) == 0x08) {
// 						shutdown(httpd_slot_sockets[id], SHUT_RDWR);
// 						httpd_slot_sockets[id] = -1;
// 						DEBUG_MSG("closed websocket connection to slot #%d", id);
// 				} else {
// 					/* send data to terminal ... */
// 					for (int i = 0; i < n; i++) {
// 						printf("%02x ", data[i]);
// 					}
// 					printf("\n");
// 				}
// 			}
// 			if (httpd_slot_sockets[id] >= 0 && FD_ISSET(httpd_slot_sockets[id], &e_fds)) {
// 				DEBUG_MSG("exp");
// 			}
// 		}

// 		MHD_run_from_select(daemon_handle, &r_fds, &w_fds, &e_fds);

// 		/* check tx fd sets */
// 		for (int id = 0; id < SLOT_COUNT; id++) {
// 			int fd = slot_fd_tx(id);
// 			if (FD_ISSET(fd, &r_fds)) {
// 				n = read(fd, data, sizeof(data));
// 				for (int j = 0; j < n; j++) {
// 					if (isprint(data[j]) || iscntrl(data[j])) {
// 						printf("%c", data[j]);
// 					} else if (data[j] != 0) {
// 						printf("{%02x}", data[j]);
// 					}
// 				}
// 				httpd_websocket_send(id, data, n, 0x02);
// 			}
// 		}
// 	}

// 	DEBUG_MSG("http daemon thread exit");
// 	return NULL;
// }

static int httpd_request_handler(void *cls, struct MHD_Connection *connection,
                                 const char *url,
                                 const char *method, const char *version,
                                 const char *upload_data,
                                 size_t *upload_data_size, void **con_cls)
{
	/* loop through list of urls to catch */
	for (struct httpd_url_regex *u = url_first; u; u = u->next) {
		char **substrings = malloc(sizeof(*substrings) * URL_MAX_REGEX_SUBSTRINGS);
		int substrings_c = 0;
		if (u->mrex && pcre_exec(u->mrex, NULL, method, strlen(method), 0, 0, NULL, 0)) {
			continue;
		}
		for (int i = 0; i < URL_MAX_REGEX_SUBSTRINGS; i++) {
			substrings[i] = NULL;
		}
		if (u->urex) {
			int ov[URL_MAX_REGEX_SUBSTRINGS];
			substrings_c = pcre_exec(u->urex, NULL, url, strlen(url), 0, 0, ov, URL_MAX_REGEX_SUBSTRINGS);
			if (substrings_c < 0) {
				continue;
			}
			for (int i = 0; i < substrings_c; i++) {
				substrings[i] = strndup(url + ov[2 * i], ov[2 * i + 1] - ov[2 * i]);
			}
		}
		/* match */
		int ret = u->callback(connection, url, method, upload_data, upload_data_size, (const char **)substrings, substrings_c, u->userdata);
		for (int i = 0; i < substrings_c; i++) {
			free(substrings[i]);
		}
		free(substrings);
		return ret;
	}

	/* only GET can get through here */
	if (strcmp(method, "GET") != 0) {
		return httpd_404(connection, MHD_HTTP_NOT_FOUND);
	}

	/* check if this is a upgrade request */
	const char *header = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Upgrade");
	if (header && strcmp(header, "websocket") == 0) {
		return httpd_upgrade_websocket(url, connection);
	}

	/* default GET handler */
	return httpd_get_default(connection, url);
}

int httpd_init(void)
{
	return 0;
}

void httpd_quit(void)
{
	if (daemon_handle) {
		MHD_stop_daemon(daemon_handle);
		daemon_handle = NULL;
	}
	if (www_root) {
		free(www_root);
		www_root = NULL;
	}
	LL_GET_LOOP(url_first, url_last, {
		loop_item->mrex ? pcre_free(loop_item->mrex) : NULL;
		loop_item->urex ? pcre_free(loop_item->urex) : NULL;
		free(loop_item);
	});
}

int httpd_start(char *address, uint16_t port, char *root)
{
	www_root = strdup(root);
	daemon_handle = MHD_start_daemon(MHD_USE_EPOLL_INTERNAL_THREAD | MHD_USE_ERROR_LOG | MHD_ALLOW_UPGRADE,
	                                 port, NULL, NULL,
	                                 &httpd_request_handler, NULL, MHD_OPTION_END);
	return daemon_handle ? 0 : -1;
}

int httpd_register_url(char *method_pattern, char *url_pattern,
                       int (*callback)(struct MHD_Connection *connection, const char *url, const char *method, const char *upload_data, size_t *upload_data_size, const char **substrings, size_t substrings_c, void *userdata),
                       void *userdata)
{
	const char *emsg;
	int eoff;
	struct httpd_url_regex *u = malloc(sizeof(*u));
	if (!u) {
		return -1;
	}
	memset(u, 0, sizeof(*u));

	/* compile both patterns */
	if (method_pattern) {
		u->mrex = pcre_compile(method_pattern, 0, &emsg, &eoff, NULL);
		if (!u->mrex) {
			free(u);
			fprintf(stderr, "method pattern pcre_compile() failed, pattern: %s, message: %s\n", method_pattern, emsg);
			return -1;
		}
	}
	if (url_pattern) {
		u->urex = pcre_compile(url_pattern, 0, &emsg, &eoff, NULL);
		if (!u->urex) {
			free(u);
			fprintf(stderr, "url pattern pcre_compile() failed, pattern: %s, message: %s\n", url_pattern, emsg);
			return -1;
		}
	}

	/* setup callback */
	u->callback = callback;
	u->userdata = userdata;

	/* append to list */
	LL_APP(url_first, url_last, u);

	return 0;
}

int httpd_websocket_send(int fd, void *data, size_t size, uint8_t opcode)
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
