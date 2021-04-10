
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "linkedlist.h"
#include "httpd.h"
#include "ws.h"


struct httpd_url_regex {
	int type;

	pcre *mrex;
	pcre *urex;

	int (*callback)(struct MHD_Connection *connection, const char *url, const char *method, const char *upload_data, size_t *upload_data_size, const char **substrings, size_t substrings_c, void *userdata);
	void *userdata;

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

static int httpd_request_handler(void *cls, struct MHD_Connection *connection,
                                 const char *url,
                                 const char *method, const char *version,
                                 const char *upload_data,
                                 size_t *upload_data_size, void **con_cls)
{
	char *substrings[URL_MAX_REGEX_SUBSTRINGS];
	int substrings_c;
	int ret = 9999;

	memset(substrings, 0, sizeof(substrings));

	/* loop through list of urls to catch */
	for (struct httpd_url_regex *u = url_first; u; u = u->next) {
		/* reset substrings */
		for (int i = 0; i < URL_MAX_REGEX_SUBSTRINGS; i++) {
			if (substrings[i]) {
				free(substrings[i]);
			}
			substrings[i] = NULL;
		}
		substrings_c = 0;

		/* method mathing */
		if (u->mrex && pcre_exec(u->mrex, NULL, method, strlen(method), 0, 0, NULL, 0)) {
			continue;
		}

		/* url matching */
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

		/* set ret to not MHD_YES or MHD_NO so the loop will continue if nothing was done*/
		ret = 9999;

		/* match */
		if (u->type == HTTPD_URL_REGEX_TYPE_NORMAL) {
			ret = u->callback(connection, url, method, upload_data, upload_data_size, (const char **)substrings, substrings_c, u->userdata);
		} else if (u->type == HTTPD_URL_REGEX_TYPE_WEBSOCKET) {
			const char *header = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Upgrade");
			if (header && strcmp(header, "websocket") == 0) {
				ret = ws_upgrade_init(url, connection);
			}
		}

		/* if ret is either MHD_YES or MHD_NO, we are done */
		if (ret == MHD_YES || ret == MHD_NO) {
			break;
		}
	}

	/* free resources */
	for (int i = 0; i < URL_MAX_REGEX_SUBSTRINGS && substrings[i]; i++) {
		free(substrings[i]);
	}

	/* if match was found */
	if (ret == MHD_YES || ret == MHD_NO) {
		return ret;
	}

	/* only GET can get through here */
	if (strcmp(method, "GET") != 0) {
		return httpd_404(connection, MHD_HTTP_NOT_FOUND);
	}

	/* default GET handler */
	return httpd_get_default(connection, url);
}

int httpd_init(void)
{
	if (ws_init()) {
		return -1;
	}
	return 0;
}

void httpd_quit(void)
{
	ws_quit();
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
	if (!daemon_handle) {
		return -1;
	}

	ws_start();

	return 0;
}

int httpd_register_url_with_type(int type, char *method_pattern, char *url_pattern,
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
	u->type = type;

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

int httpd_register_url(char *method_pattern, char *url_pattern,
                       int (*callback)(struct MHD_Connection *connection, const char *url, const char *method, const char *upload_data, size_t *upload_data_size, const char **substrings, size_t substrings_c, void *userdata),
                       void *userdata)
{
	return httpd_register_url_with_type(HTTPD_URL_REGEX_TYPE_NORMAL, method_pattern, url_pattern, callback, userdata);
}
