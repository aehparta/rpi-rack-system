/*
 * Rack system master.
 */

#include <libe/libe.h>
#include <libe/linkedlist.h>
#include "slot.h"
#include "opt.h"
#include "httpd.h"


#define SLOT_COUNT          12
#define SPI_FREQ            400000


struct spi_master master;
struct spi_device device;

struct opt_option opt_all[] = {
	{ 'h', "help", no_argument, 0, NULL, NULL, "display this help and exit", { 0 } },

	/* telnet server */
	{ 'S', "slot-port", required_argument, 0, "0", NULL, "base port for slot connections, disabled as default", { OPT_FILTER_INT, 1, 65535 } },

	/* http server */
	{ 'P', "http-port", required_argument, 0, "80", NULL, "http server port", { OPT_FILTER_INT, 1, 65535 } },
	{ 'D', "html", required_argument, 0, "/var/www/rack", NULL, "public html directory", { 0 } },

	{ 0, 0, 0, 0, 0, 0, 0, { 0 } }
};


int p_init(void)
{
	/* library initializations */
	os_init();
	log_init();

	/* init and open slots */
	slot_init(SLOT_COUNT);
	int base_port = opt_get_int('S');
	for (int i = 0; i < SLOT_COUNT; i++) {
		CRIT_IF_R(slot_open(i, "0.0.0.0", base_port > 0 ? base_port + i : 0), -1, "unable to open slot #%d", i);
	}

	/* open spi as master */
	spi_master_open(&master, NULL, SPI_FREQ, 0, 0, 0);
	spi_open(&device, &master, 0);

	/* start http server */
	CRIT_IF_R(httpd_init(NULL, opt_get_int('P'), opt_get('D')), -1, "unable to start httpd server");

	return 0;
}

void p_exit(int signum)
{
	httpd_quit();
	for (int i = 0; i < SLOT_COUNT; i++) {
		slot_close(i);
	}
	spi_close(&device);
	spi_master_close(&master);
	log_quit();
	os_quit();
	exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	/* parse options */
	IF_R(opt_init(opt_all, NULL, NULL, NULL) || opt_parse(argc, argv), EXIT_FAILURE);

	/* basic initialization */
	signal(SIGINT, p_exit);
	ERROR_IF_R(p_init(), -1, "base initialization failed");

	/* start program loop */
	INFO_MSG("this thing started");
	int sn = 0;
	while (1) {
		struct slot_cq *cq;
		char data[8], data_send[sizeof(data) + 1];

		memset(data, 0, sizeof(data));
		memset(data_send, 0, sizeof(data_send));

		/* check if there is input to be forwarded */
		// pthread_mutex_lock(&slots[sn].qlock);
		// LL_GET(slots[sn].qfirst, slots[sn].qlast, cq);
		// pthread_mutex_unlock(&slots[sn].qlock);
		// if (cq) {
		// 	data[0] = cq->c;
		// 	free(cq);
		// }

		spi_transfer(&device, (uint8_t *)data, sizeof(data));

		for (int i = 0, j = 0; i < sizeof(data); i++) {
			if (isprint(data[i]) || iscntrl(data[i])) {
				printf("%c", data[i]);
			} else if (data[i] != 0) {
				printf("{%02x}", data[i]);
			}

			if (data[i] != 0) {
				data_send[j++] = data[i];
			}
		}

		if (strlen(data_send) > 0) {
			slot_send(sn, data_send, strlen(data_send));
		}

		if (data[sizeof(data) - 1] == '\0') {
			os_sleepf(0.001);
		}
	}

	return EXIT_SUCCESS;
}
