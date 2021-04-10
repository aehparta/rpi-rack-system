/*
 * Slot handling.
 */

#include <string.h>
#include <libe/libe.h>
#include <libe/linkedlist.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include "slot.h"
#include "opt.h"


struct slot slots[SLOT_COUNT];

/* internals */
static void *slot_telnet_thread(void *data);

#define SLOT_GET(id, ret) \
	if (id < 0 || id >= SLOT_COUNT) { return ret; }; \
	struct slot *slot = &slots[id];


int slot_init()
{
	memset(slots, 0, sizeof(slots));

	for (int i = 0; i < SLOT_COUNT; i++) {
		struct slot *slot = &slots[i];
		slot->id = i;
		slot->server_fd = -1;
		slot->client_fd = -1;
		slot->p_rx[0] = -1;
		slot->p_rx[1] = -1;
		slot->p_tx[0] = -1;
		slot->p_tx[1] = -1;
	}

	int base_port = opt_get_int('S');
	for (int i = 0; i < SLOT_COUNT; i++) {
		CRIT_IF_R(slot_open(i, "0.0.0.0", base_port > 0 ? base_port + i : 0), -1, "unable to open slot #%d", i);
	}

	return 0;
}

void slot_quit()
{
	for (int i = 0; i < SLOT_COUNT; i++) {
		slot_close(i);
	}
}

int slot_open(int id, char *address, uint16_t port)
{
	SLOT_GET(id, -1);

	/* create pipe for sending data through slot rx line */
	CRIT_IF_R(pipe2(slot->p_rx, O_NONBLOCK), -1, "failed to create pipe for sending data through slot #%d rx line", id);
	/* create pipe for receiving data through slot tx line */
	CRIT_IF_R(pipe2(slot->p_tx, O_NONBLOCK), -1, "failed to create pipe for receiving data through slot #%d tx line", id);

	if (port > 0) {
		struct addrinfo hints, *result, *p;
		int sock = -1;
		int err;
		int yes = 1;

		char port_s[8];
		sprintf(port_s, "%d", port);

		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		err = getaddrinfo(address, port_s, &hints, &result);
		CRIT_IF_R(err, -1, "getaddrinfo failed, reason: %s\n", gai_strerror(err));

		for (p = result; p != NULL; p = p->ai_next) {
			sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
			if (sock < 0) {
				continue;
			}
			setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
			if (bind(sock, p->ai_addr, p->ai_addrlen) == -1) {
				close(sock);
				continue;
			}
			break;
		}
		freeaddrinfo(result);

		CRIT_IF_R(!p, -1, "failed to open socket for slot #%d", id);

		slot->server_fd = sock;
		err = listen(slot->server_fd, 1);
		CRIT_IF_R(err, -1, "failed to open listening socket for slot #%d", id);

		/* start handler thread */
		DEBUG_MSG("starting telnet thread for slot #%d (%s:%d)", slot->id, address ? address : "?", port);
		slot->thread_telnet_exec = 1;
		pthread_create(&slot->thread_telnet, NULL, slot_telnet_thread, slot);
	}

	return 0;
}

void slot_close(int id)
{
	SLOT_GET(id,);

	if (slot->thread_telnet_exec) {
		slot->thread_telnet_exec = 0;
		pthread_join(slot->thread_telnet, NULL);
	}

	if (slot->client_fd >= 0) {
		close(slot->client_fd);
		slot->client_fd = -1;
	}
	if (slot->server_fd >= 0) {
		close(slot->server_fd);
		slot->server_fd = -1;
	}

	if (slot->p_rx[0] >= 0) {
		close(slot->p_rx[0]);
		close(slot->p_rx[1]);
		slot->p_rx[0] = -1;
		slot->p_rx[1] = -1;
	}

	if (slot->p_tx[0] >= 0) {
		close(slot->p_tx[0]);
		close(slot->p_tx[1]);
		slot->p_tx[0] = -1;
		slot->p_tx[1] = -1;
	}
}

int slot_spi_check(int id, struct spi_device *device)
{
	SLOT_GET(id, -1);
	char data[8], data_send[sizeof(data) + 1];
	int size;

	memset(data, 0, sizeof(data));
	memset(data_send, 0, sizeof(data_send));

	size = read(slot->p_rx[0], data, 1);
	data[0] = size == 1 ? data[0] : '\0';

	spi_transfer(device, (uint8_t *)data, sizeof(data));

	size = 0;
	for (int i = 0; i < sizeof(data); i++) {
		// if (isprint(data[i]) || iscntrl(data[i])) {
		// 	printf("%c", data[i]);
		// } else if (data[i] != 0) {
		// 	printf("{%02x}", data[i]);
		// }

		if (data[i] != 0) {
			data_send[size++] = data[i];
		}
	}

	if (size > 0) {
		if (slot->client_fd >= 0) {
			send(slot->client_fd, data_send, size, 0);
		}
		write(slot->p_tx[1], data_send, size);
	}

	return 0;
}

int slot_fd_rx(int id)
{
	SLOT_GET(id, -1);
	return slot->p_rx[1];
}

int slot_fd_tx(int id)
{
	SLOT_GET(id, -1);
	return slot->p_tx[0];
}


/* internals */


static void *slot_telnet_thread(void *data)
{
	struct slot *slot = data;
	fd_set r_fds;
	int max_fd = -1;

	FD_ZERO(&r_fds);
	FD_SET(slot->server_fd, &r_fds);
	max_fd = slot->server_fd;

	while (slot->thread_telnet_exec) {
		struct timeval tv;
		fd_set r = r_fds;
		int n;

		/* 100 ms timeout */
		tv.tv_sec = 0;
		tv.tv_usec = 100000;

		/* wait */
		n = select(max_fd + 1, &r, NULL, NULL, &tv);
		if (n < 0) {
			CRIT_MSG("select() failed, reason: %s", strerror(errno));
			break;
		} else if (n == 0) {
			/* timeout */
			continue;
		}

		/* if data received */
		if (slot->client_fd >= 0 && FD_ISSET(slot->client_fd, &r)) {
			char data[1024];
			memset(data, 0, sizeof(data));
			n = recv(slot->client_fd, data, sizeof(data), 0);

			/* if connection closed */
			if (n <= 0) {
				int s = slot->client_fd;
				slot->client_fd = -1;
				close(s);
				FD_CLR(s, &r_fds);
				max_fd = slot->server_fd;
				printf("client disconnected on slot #%d\n", slot->id);
			} else {
				/* actual data received */
				printf("got data: ");
				for (int i = 0; i < n; i++) {
					if (isprint(data[i])) {
						printf("%c", data[i]);
					} else {
						printf("[%d]", data[i]);
					}

				}
				printf("\n");

				write(slot->p_rx[1], data, n);
			}
		}

		/* if new connection */
		if (FD_ISSET(slot->server_fd, &r)) {
			int s = accept(slot->server_fd, NULL, NULL);
			if (slot->client_fd >= 0) {
				/* already open connection, deny this */
				dprintf(s, "telnet connection already open on slot #%d\n", slot->id);
				close(s);
			} else {
				slot->client_fd = s;
				FD_SET(slot->client_fd, &r_fds);
				max_fd = max_fd < slot->client_fd ? slot->client_fd :  max_fd;
				INFO_MSG("telnet connection opened to slot #%d", slot->id);
			}
		}
	}

	DEBUG_MSG("exit telnet thread for slot #%d", slot->id);

	return NULL;
}
