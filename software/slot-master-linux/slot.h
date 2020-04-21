/*
 * Slot handling.
 */

#ifndef _SLOT_H_
#define _SLOT_H_

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

	pthread_t thread;
	uint8_t thread_exec;

	struct slot_cq *qfirst;
	struct slot_cq *qlast;
	pthread_mutex_t qlock;
};


int slot_init(struct slot *slot);

int slot_open(struct slot *slot, int id, char *address, uint16_t port);
void slot_close(struct slot *slot);

int slot_send(struct slot *slot, void *data, size_t size);



#endif /* _SLOT_H_ */
