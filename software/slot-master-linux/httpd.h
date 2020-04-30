/*
 * HTTP daemon.
 */

#ifndef _HTTPD_H_
#define _HTTPD_H_

#include <stdint.h>
#include <pcre.h>
#include <microhttpd.h>

int httpd_init(void);
void httpd_quit(void);
int httpd_start(char *address, uint16_t port, char *root);

int httpd_register_url(char *method_pattern, char *url_pattern,
                       int (*callback)(struct MHD_Connection *connection, const char *url, const char *method, const char *upload_data, size_t *upload_data_size, const char **substrings, size_t substrings_c, void *userdata),
                       void *userdata);


/* internal stuff */

int httpd_register_url_with_type(int type, char *method_pattern, char *url_pattern,
                                 int (*callback)(struct MHD_Connection *connection, const char *url, const char *method, const char *upload_data, size_t *upload_data_size, const char **substrings, size_t substrings_c, void *userdata),
                                 void *userdata);

#define URL_MAX_REGEX_SUBSTRINGS 30 /* should be multiple of 3 */
enum {
	HTTPD_URL_REGEX_TYPE_NORMAL = 0,
	HTTPD_URL_REGEX_TYPE_WEBSOCKET
};

#endif /* _HTTPD_H_ */
