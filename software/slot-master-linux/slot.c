/*
 * Slot handling.
 */

#include <string.h>
#include <libe/libe.h>
#include <libe/linkedlist.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include "slot.h"


/* internals */
static void *slot_thread(void *data);


int slot_init(struct slot *slot)
{
	memset(slot, 0, sizeof(*slot));
	slot->server_fd = -1;
	slot->client_fd = -1;
	pthread_mutex_init(&slot->qlock, NULL);
	return 0;
}

int slot_open(struct slot *slot, int id, char *address, uint16_t port)
{
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

	slot->id = id;

	/* start handler thread */
	slot->thread_exec = 1;
	pthread_create(&slot->thread, NULL, slot_thread, slot);

	return 0;
}

void slot_close(struct slot *slot)
{
	slot->thread_exec = 0;
	pthread_join(slot->thread, NULL);

	if (slot->client_fd >= 0) {
		close(slot->client_fd);
		slot->client_fd = -1;
	}
	if (slot->server_fd >= 0) {
		close(slot->server_fd);
		slot->server_fd = -1;
	}

	for (struct slot_cq *q = slot->qfirst; q; ) {
		struct slot_cq *qq = q->next;
		free(q);
		q = qq;
	}
	slot->qfirst = NULL;
	slot->qlast = NULL;
	pthread_mutex_destroy(&slot->qlock);
}

int slot_send(struct slot *slot, void *data, size_t size)
{
	if (slot->client_fd >= 0) {
		return send(slot->client_fd, data, size, 0);
	}

	return -1;
}

/* internals */


static void *slot_thread(void *data)
{
	struct slot *slot = data;
	fd_set r_fds;
	int max_fd = -1;

	FD_ZERO(&r_fds);
	FD_SET(slot->server_fd, &r_fds);
	max_fd = slot->server_fd;

	while (slot->thread_exec) {
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
				close(slot->client_fd);
				FD_CLR(slot->client_fd, &r_fds);
				slot->client_fd = -1;
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

					pthread_mutex_lock(&slot->qlock);
					struct slot_cq *cq = malloc(sizeof(*cq));
					cq->c = data[i];
					LL_APP(slot->qfirst, slot->qlast, cq);
					pthread_mutex_unlock(&slot->qlock);
				}
				printf("\n");
			}
		}

		/* if new connection */
		if (FD_ISSET(slot->server_fd, &r)) {
			int s = accept(slot->server_fd, NULL, NULL);
			if (slot->client_fd >= 0) {
				/* already open connection, deny this */
				dprintf(s, "connection already open on slot #%d\n", slot->id);
				close(s);
			} else {
				slot->client_fd = s;
				FD_SET(slot->client_fd, &r_fds);
				max_fd = max_fd < slot->client_fd ? slot->client_fd :  max_fd;
				printf("connection opened to slot #%d\n", slot->id);
			}
		}
	}

	printf("exit thread of slot #%d\n", slot->id);

	return NULL;
}
