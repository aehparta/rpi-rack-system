/*
 * Slot handling.
 */

#ifndef _SLOT_H_
#define _SLOT_H_

#include <microhttpd.h>
#include <stdint.h>
#include <pthread.h>


struct slot_cq {
	char c;
	struct slot_cq *prev;
	struct slot_cq *next;
};

struct slot {
	int id;
	
	int server_fd;
	int client_fd;
	MHD_socket websocket;

	pthread_t thread_telnet;
	uint8_t thread_telnet_exec;

	struct slot_cq *qfirst;
	struct slot_cq *qlast;
	pthread_mutex_t qlock;
};


int slot_init(int count);

int slot_open(int id, char *address, uint16_t port);
void slot_close(int id);

int slot_send(int id, void *data, size_t size);

int slot_add_websocket(int id, MHD_socket s);



#endif /* _SLOT_H_ */
