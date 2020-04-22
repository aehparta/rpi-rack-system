/*
 * HTTP daemon.
 */

#ifndef _HTTPD_H_
#define _HTTPD_H_

#include <stdint.h>

int httpd_init(char *address, uint16_t port, char *root);
void httpd_quit(void);

#endif /* _HTTPD_H_ */
