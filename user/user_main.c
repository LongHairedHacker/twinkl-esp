#include <stdio.h>

#include "ets_sys.h"
#include "ip_addr.h"
#include "espconn.h"
#include "osapi.h"
#include "wifi.h"
#include "gpio.h"
#include "mem.h"
#include "user_interface.h"

#include "pin_map.h"
#include "debug.h"
#include "driver/uart.h"
#include "twinkl.h"


#define SPI_TASK_PRIO 0
#define SPI_TASK_QUEUE_LENGTH 1

#define SPI_CHANNEL_COUNT 3

const uint16_t spi_addresses[SPI_CHANNEL_COUNT] = { 420, 421, 422};


struct espconn *udp_server;

volatile uint8_t update_scheduled = 0;
os_event_t spi_task_queue[SPI_TASK_QUEUE_LENGTH];
os_timer_t spi_update_timer;

uint8_t channels[TWINKL_CHANNEL_COUNT];


void ICACHE_FLASH_ATTR udpserver_recv(void *arg, char *data, unsigned short len)
{
	INFO("Received %d bytes\n", len);

	if(len == sizeof(struct twinkl_message)) {
		uint8_t changed_already;

		INFO("Data has the size of a twinkl message\n");
		twinkl_process_message((struct twinkl_message *) data);
		INFO("Twinkl message has been processed\n");

		if(!update_scheduled) {
			update_scheduled = 1;
			system_os_post(SPI_TASK_PRIO, 0, 0);	
		}
	}

}


void ICACHE_FLASH_ATTR wifiConnectCb(uint8_t status) {
	if(status == STATION_GOT_IP){
		INFO("Wifi connection established\n");
		udp_server = (struct espconn *) os_zalloc(sizeof(struct espconn));
		udp_server->type = ESPCONN_UDP;
		udp_server->proto.udp = (esp_udp *) os_zalloc(sizeof(esp_udp));
		udp_server->proto.udp->local_port = udp_port;
		espconn_regist_recvcb(udp_server, udpserver_recv);

		espconn_create(udp_server);
	}
	else if(udp_server != NULL) {
		espconn_delete(udp_server);
		os_free(udp_server);
		udp_server = NULL;
	}
}


void ICACHE_FLASH_ATTR spi_task(os_event_t *events) {
	int i;
	update_scheduled = 0;
	
	if(twinkl_has_changes()) {
		INFO("Updating channels\n");
		twinkl_render(channels);
	}

	//INFO("Sending channels\n");

	
	for(i = 0; i < SPI_CHANNEL_COUNT; i++) {
		spibang_send_byte(channels[spi_addresses[i]]);	
	}

	//INFO("Done sending channels\n");
	os_timer_arm(&spi_update_timer, 250, 0);
}


void spi_update(void *arg) {
	if(!update_scheduled) {
		update_scheduled = 1;
		system_os_post(SPI_TASK_PRIO, 0, 0);
	}
}

void user_init(void) {
	uart_init(BIT_RATE_115200, BIT_RATE_250000);
	os_delay_us(1000000);

	update_scheduled = 0;
	memset(channels, 0, TWINKL_CHANNEL_COUNT);

	gpio_init();

	twinkl_init();

	spibang_init();

	os_timer_disarm(&spi_update_timer);
	os_timer_setfn(&spi_update_timer, (os_timer_func_t *)spi_update, NULL);
	os_timer_arm(&spi_update_timer, 250, 0);

	system_os_task(spi_task, SPI_TASK_PRIO, spi_task_queue, SPI_TASK_QUEUE_LENGTH);

	WIFI_Connect(wifi_ssid, wifi_password, wifiConnectCb);
}
