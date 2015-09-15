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


#define DMX_TASK_PRIO 0
#define DMX_TASK_QUEUE_LENGTH 1


struct espconn *udp_server;

const int dmx_tx_pin = 2;
const uint16_t dmx_refresh_delay = 10;

volatile uint8_t update_scheduled = 0;
os_event_t dmx_task_queue[DMX_TASK_QUEUE_LENGTH];
os_timer_t dmx_update_timer;

uint8_t dmx_channels[TWINKL_CHANNEL_COUNT];


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
			system_os_post(DMX_TASK_PRIO, 0, 0);	
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


void ICACHE_FLASH_ATTR dmx_task(os_event_t *events) {
	int i;
	update_scheduled = 0;
	
	if(twinkl_has_changes()) {
		INFO("Updating DMX channels\n");
		twinkl_render(dmx_channels);
	}

	//INFO("Sending DMX channels\n");

	//Space for break
	PIN_FUNC_SELECT(pin_mux[dmx_tx_pin], FUNC_GPIO2);	
	gpio_output_set(0, BIT2, BIT2, 0); 
	os_delay_us(125);
	
	//Mark After Break
	gpio_output_set(BIT2, 0, BIT2, 0);
	os_delay_us(50);

	//Looks the wrong way round, but reduces jitter somehow
	//Do not touch.
	PIN_FUNC_SELECT(pin_mux[dmx_tx_pin], FUNC_U1TXD_BK);	
	uart_tx_one_char(1, 0);

	for(i = 0; i < TWINKL_CHANNEL_COUNT; i++) {
		uart_tx_one_char(1, dmx_channels[i]);
	}

	//INFO("Done sending DMX channels\n");
	os_timer_arm(&dmx_update_timer, dmx_refresh_delay, 0);
}


void dmx_update(void *arg) {
	if(!update_scheduled) {
		update_scheduled = 1;
		system_os_post(DMX_TASK_PRIO, 0, 0);
	}
}

void user_init(void) {
	uart_init(BIT_RATE_115200, BIT_RATE_250000);
	os_delay_us(1000000);

	update_scheduled = 0;
	memset(dmx_channels, 127, TWINKL_CHANNEL_COUNT);

	gpio_init();

	twinkl_init();

	os_timer_disarm(&dmx_update_timer);
	os_timer_setfn(&dmx_update_timer, (os_timer_func_t *)dmx_update, NULL);
	os_timer_arm(&dmx_update_timer, dmx_refresh_delay, 0);

	system_os_task(dmx_task, DMX_TASK_PRIO, dmx_task_queue, DMX_TASK_QUEUE_LENGTH);

	WIFI_Connect(wifi_ssid, wifi_password, wifiConnectCb);
}
