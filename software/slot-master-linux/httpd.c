
#include <libe/libe.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <microhttpd.h>
#include <b64/cdecode.h>
#include <b64/cencode.h>
#include <openssl/sha.h>
#include "httpd.h"
#include "opt.h"
#include "slot.h"


static struct MHD_Daemon *httpd_daemon;
static char *httpd_root = NULL;


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
	slot_add_websocket(0, sock);
}

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

static int httpd_request_handler(void *cls, struct MHD_Connection *connection,
                                 const char *url,
                                 const char *method, const char *version,
                                 const char *upload_data,
                                 size_t *upload_data_size, void **con_cls)
{
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
	httpd_daemon = MHD_start_daemon(MHD_USE_ERROR_LOG | MHD_USE_POLL_INTERNAL_THREAD | MHD_ALLOW_UPGRADE,
	                                port, NULL, NULL,
	                                &httpd_request_handler, NULL, MHD_OPTION_END);
	CRIT_IF_R(!httpd_daemon, -1, "unable to initialize development device front end http daemon");
	NOTICE_MSG("listening to http requests at http://%s:%d", address ? address : "0.0.0.0", port);
	httpd_root = strdup(root);
	return 0;
}

void httpd_quit(void)
{
	if (httpd_daemon) {
		MHD_stop_daemon(httpd_daemon);
		httpd_daemon = NULL;
	}
	if (httpd_root) {
		free(httpd_root);
		httpd_root = NULL;
	}
}
