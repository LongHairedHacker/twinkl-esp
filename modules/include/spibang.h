#ifndef SPIBANG_H
#define SPIBANG_H

#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"

#include "pin_map.h"


void ICACHE_FLASH_ATTR spibang_init(void);
void ICACHE_FLASH_ATTR spibang_send_byte(uint8_t byte);


#endif

