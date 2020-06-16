/*
 * Rack system master.
 */

#include <libe/libe.h>
#include <libe/linkedlist.h>
#include <pthread.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>


#define SLOT_COUNT          12
#define SPI_FREQ            400000
#define TELNET_BASE_PORT    2000

struct q {
	char c;
	struct q *prev;
	struct q *next;
};

struct slot {
	int s_accept;
	int s_client;

	struct q *qfirst;
	struct q *qlast;
	pthread_mutex_t qlock;
};

struct slot slots[SLOT_COUNT];

struct spi_master master;
struct spi_device device;


int p_init(void)
{
	int err = 0, yes = 1;
	/* library initializations */
	os_init();
	log_init();

	/* reset slots */
	memset(slots, 0, sizeof(slots));
	for (int i = 0; i < SLOT_COUNT; i++) {
		slots[i].s_accept = -1;
		slots[i].s_client = -1;
	}
	for (int i = 0; i < SLOT_COUNT; i++) {
		struct addrinfo hints, *servinfo, *p;
		int sockfd = -1;

		char port[8];
		sprintf(port, "%d", TELNET_BASE_PORT + i);

		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		err = getaddrinfo("0.0.0.0", port, &hints, &servinfo);
		CRIT_IF_R(err, -1, "getaddrinfo failed, reason: %s\n", gai_strerror(err));

		for (p = servinfo; p != NULL; p = p->ai_next) {
			sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
			if (sockfd < 0) {
				continue;
			}
			setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
			if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
				close(sockfd);
				sockfd = -1;
				continue;
			}
			break;
		}

		CRIT_IF_R(sockfd < 0, -1, "failed to open socket for slot #%d", i);

		slots[i].s_accept = sockfd;
		err = listen(slots[i].s_accept, 1);
		CRIT_IF_R(err, -1, "failed to open listening socket for slot #%d", i);
	}

	/* open spi as master */
	spi_master_open(&master, NULL, SPI_FREQ, 0, 0, 0);
	spi_open(&device, &master, 0);

	return 0;
}

void p_exit(int signum)
{
	for (int i = 0; i < SLOT_COUNT; i++) {
		if (slots[i].s_client >= 0) {
			close(slots[i].s_client);
		}
		if (slots[i].s_accept >= 0) {
			close(slots[i].s_accept);
		}
		for (struct q *q = slots[i].qfirst; q; ) {
			struct q *qq = q->next;
			free(q);
			q = qq;
		}
	}
	spi_close(&device);
	spi_master_close(&master);
	log_quit();
	os_quit();
	exit(0);
}

int main(void)
{
	signal(SIGINT, p_exit);

	ERROR_IF_R(p_init(), -1, "base initialization failed");

	INFO_MSG("this thing started");
	int sn = 0;
	while (1) {
		struct q *q;
		uint8_t data[8];
		memset(data, 0, sizeof(data));

		/* check if there is input to be forwarded */
		pthread_mutex_lock(&slots[sn].qlock);
		LL_GET(slots[sn].qfirst, slots[sn].qlast, q);
		pthread_mutex_unlock(&slots[sn].qlock);
		if (q) {
			data[0] = q->c;
			free(q);
		}

		spi_transfer(&device, data, sizeof(data));

		for (int i = 0; i < sizeof(data); i++) {
			if (isprint(data[i]) || iscntrl(data[i])) {
				printf("%c", data[i]);
			} else if (data[i] != 0) {
				printf("?");
			}
		}

		if (data[sizeof(data) - 1] == '\0') {
			os_sleepf(0.001);
		}
	}

	return 0;
}
