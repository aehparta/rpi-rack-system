/*
 * Rack system master.
 */

#include <libe/libe.h>
#include <libe/linkedlist.h>
#include "slot.h"
#include "opt.h"
#include "httpd.h"
#include "ws.h"


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
	CRIT_IF_R(httpd_init(), -1, "failed to initialize http server");
	CRIT_IF_R(httpd_start(NULL, opt_get_int('P'), opt_get('D')), -1, "unable to start httpd server");

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


int twwwcb(struct MHD_Connection *connection, const char *url, const char *method, const char *upload_data, size_t *upload_data_size, const char **substrings, size_t substrings_c, void *userdata)
{
	char data[1024];

	sprintf(data, "got request, method: %s, url: %s\n", method, url);
	for (int i = 0; i < substrings_c; i++) {
		sprintf(data + strlen(data), "substring: %s\n", substrings[i]);
	}

	printf("%s", data);

	struct MHD_Response *response = MHD_create_response_from_buffer(strlen(data), data, MHD_RESPMEM_MUST_COPY);
	MHD_add_response_header(response, "Content-Type", "text/html; charset=utf-8");
	int err = MHD_queue_response(connection, 200, response);
	MHD_destroy_response(response);

	return err;
}

int p_ws_recv(int fd, const char *url, void *data, size_t size, void *userdata)
{
	return 0;
}

int main(int argc, char *argv[])
{
	/* parse options */
	IF_R(opt_init(opt_all, NULL, NULL, NULL) || opt_parse(argc, argv), EXIT_FAILURE);

	CRIT_IF_R(ws_register_url("/slot/[0-9]+$", NULL), 1, "failed to register");
	CRIT_IF_R(httpd_register_url(NULL, "/here/([a-z]+)/([0-9]+)/?$", twwwcb, NULL), 1, "failed to register");

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
