/*
 * Slot handling.
 */

#ifndef _SLOT_H_
#define _SLOT_H_

#include <microhttpd.h>
#include <stdint.h>
#include <pthread.h>


struct slot {
	int id;
	
	int server_fd;
	int client_fd;

	pthread_t thread_telnet;
	uint8_t thread_telnet_exec;

	/* pipe for sending data through slot rx line */
	int p_rx[2];

	/* pipe for receiving data through slot tx line */
	int p_tx[2];
};


int slot_init(void);
void slot_quit(void);

int slot_open(int id, char *address, uint16_t port);
void slot_close(int id);

int slot_spi_check(int id, struct spi_device *device);

int slot_fd_rx(int id);
int slot_fd_tx(int id);


#endif /* _SLOT_H_ */
