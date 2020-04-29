/*
 * Rack system master.
 */

#include <libe/libe.h>
#include <libe/linkedlist.h>
#include "slot.h"
#include "opt.h"
#include "httpd.h"


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
	CRIT_IF_R(slot_init(), -1, "slot initialization failed");

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
	slot_quit();
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
	while (1) {
		int c = 0;

		for (int i = 0; i < SLOT_COUNT; i++) {
			c += slot_spi_check(i, &device);
		}

		if (!c) {
			os_sleepf(0.001);
		}
	}

	return EXIT_SUCCESS;
}
