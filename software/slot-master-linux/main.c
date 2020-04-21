/*
 * Rack system master.
 */

#include <libe/libe.h>
#include <libe/linkedlist.h>
#include "slot.h"


#define SLOT_COUNT          12
#define SPI_FREQ            400000
#define TELNET_BASE_PORT    2000


struct slot slots[SLOT_COUNT];

struct spi_master master;
struct spi_device device;


int p_init(void)
{
	/* library initializations */
	os_init();
	log_init();

	/* init slots */
	memset(slots, 0, sizeof(slots));
	for (int i = 0; i < SLOT_COUNT; i++) {
		slot_init(&slots[i]);
	}
	/* open slots */
	for (int i = 0; i < SLOT_COUNT; i++) {
		slot_open(&slots[i], i, "0.0.0.0", TELNET_BASE_PORT + i);
	}

	/* open spi as master */
	spi_master_open(&master, NULL, SPI_FREQ, 0, 0, 0);
	spi_open(&device, &master, 0);

	return 0;
}

void p_exit(int signum)
{
	for (int i = 0; i < SLOT_COUNT; i++) {
		slot_close(&slots[i]);
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
		struct slot_cq *cq;
		char data[8], data_send[sizeof(data) + 1];

		memset(data, 0, sizeof(data));
		memset(data_send, 0, sizeof(data_send));

		/* check if there is input to be forwarded */
		pthread_mutex_lock(&slots[sn].qlock);
		LL_GET(slots[sn].qfirst, slots[sn].qlast, cq);
		pthread_mutex_unlock(&slots[sn].qlock);
		if (cq) {
			data[0] = cq->c;
			free(cq);
		}

		spi_transfer(&device, (uint8_t *)data, sizeof(data));

		for (int i = 0, j = 0; i < sizeof(data); i++) {
			if (isprint(data[i]) || iscntrl(data[i])) {
				printf("%c", data[i]);
			} else if (data[i] != 0) {
				printf("?");
			}

			if (data[i] != 0) {
				data_send[j++] = data[i];
			}
		}

		if (strlen(data_send) > 0) {
			slot_send(&slots[sn], data_send, strlen(data_send));
		}

		if (data[sizeof(data) - 1] == '\0') {
			os_sleepf(0.001);
		}
	}

	return 0;
}
