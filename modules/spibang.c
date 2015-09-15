#include "spibang.h"

const int clock_pin = 15;
const int data_pin = 13;

const int spi_period = 10;


void ICACHE_FLASH_ATTR spibang_init(void) {
	PIN_FUNC_SELECT(pin_mux[clock_pin], pin_func[clock_pin]);
	PIN_FUNC_SELECT(pin_mux[data_pin], pin_func[data_pin]);
}


void ICACHE_FLASH_ATTR spibang_send_byte(uint8_t byte) {
	int bit;
	for(bit = 7; bit >= 0; bit--) {
		GPIO_OUTPUT_SET(clock_pin,0);
		if(byte & (1 << bit)) {
			GPIO_OUTPUT_SET(data_pin,1);
		}
		else {
			GPIO_OUTPUT_SET(data_pin,0);
		}

		os_delay_us(spi_period/2);

		GPIO_OUTPUT_SET(clock_pin,1);

		os_delay_us(spi_period/2);
	}

	GPIO_OUTPUT_SET(data_pin,0);
	GPIO_OUTPUT_SET(clock_pin,0);
}

