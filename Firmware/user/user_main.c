/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: user_main.c
 *
 * Description: entry file of user application
 *
 * Modification history:
 *     2015/1/23, v1.0 create this file.
*******************************************************************************/

#include "osapi.h"
#include "at_custom.h"
#include "user_interface.h"

void user_rf_pre_init(void)
{
}

void user_init(void)
{
    char buf[64] = {0};
    at_customLinkMax = 5;
	
    at_init();
	shield_init(); // Shield init must be called _after_ at_init() - to set baud rate
	
    os_sprintf(buf,"compile time:%s %s",__DATE__,__TIME__);
    at_set_custom_info(buf);
    at_port_print("\r\n");
	at_port_print(WELCOME_MESSAGE);
    at_port_print("\r\n");
}