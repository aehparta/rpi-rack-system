/*
 * Websockets.
 */

#ifndef _WS_H_
#define _WS_H_

#include <microhttpd.h>
#include <stdint.h>
#include <pthread.h>

int ws_init(void);
void ws_quit(void);
int ws_start(void);

int ws_register_url(char *url_pattern, void *userdata);
int ws_send(int fd, void *data, size_t size, uint8_t opcode);

int ws_upgrade_init(const char *url, struct MHD_Connection *connection);


#endif /* _WS_H_ */
