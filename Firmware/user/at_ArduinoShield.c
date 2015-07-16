/*****************************************************************************
 * AT Command to enable/disable Arduino Bootload
 *
******************************************************************************/

#include "user_interface.h"
#include "user_config.h"
#include "gpio.h"
#include "at_custom.h"
#include "osapi.h"
#include "espconn.h"
#include "mem.h"
#include "driver/uart.h"

#define RST_LISTENING_PORT	1234
#define RST_INIT_STRING		"bootloader"
#define RST_DONE_STRING		"boot"

#define MAX_RESET_STR_LEGNTH 10

static char resetCommand[MAX_RESET_STR_LEGNTH + 1] = RST_INIT_STRING;
static int resetPort = RST_LISTENING_PORT;
static bool resetEnable = true;

struct espconn *rstListenPort = NULL;
struct espconn *pCon;
static bool inBootloader = false;

UartDevice UartDev;
void shield_init(void);

// Map pins to pin functions - required to use pull-up macro's
uint32_t pinMap[] = {
	PERIPHS_IO_MUX_GPIO0_U, // 0
	0, // 1 (TX, can't change)
	PERIPHS_IO_MUX_GPIO2_U, // 2
	0, // 3 (RX, can't change)
	PERIPHS_IO_MUX_GPIO4_U, // 4
	PERIPHS_IO_MUX_GPIO5_U, // 5
	0, // 6
	0, // 7
	0, // 8
	0, // 9
	0, // 10
	0, // 11
	PERIPHS_IO_MUX_MTDI_U, // 12
	PERIPHS_IO_MUX_MTCK_U, // 13
	PERIPHS_IO_MUX_MTMS_U, // 14
	PERIPHS_IO_MUX_MTDO_U // 15
};

uint32_t pinFunction[] = {
	FUNC_GPIO0,
	0,
	FUNC_GPIO2,
	0,
	FUNC_GPIO4,
	FUNC_GPIO5,
	0,
	0,
	0,
	0,
	0,
	0,
	FUNC_GPIO12,
	FUNC_GPIO13,
	FUNC_GPIO14,
	FUNC_GPIO15
};

uint8_t pinValid(int pin)
{
	// Don't go any further if the pin is un-usable or non-existant
	if ((pin > 16) || (pinMap[pin] == 0))	
		return 0;
	
	return 1;
}

// pinMode([pin], [direction])
void ICACHE_FLASH_ATTR
at_setupPinModeCmd(uint8_t id, char *pPara)
{
    int result = 0, err = 0, flag = 0;
    uint8 buffer[32] = {0};
    pPara++; // skip '='

    //get the first parameter (uint8_t)
    // digit
    flag = at_get_next_int_dec(&pPara, &result, &err);

    // flag must be true because there are more parameters
    if ((flag == FALSE) && (result > 0) )
	{
        at_response_error();
        return;
    }
	
	uint8_t pin = result;
	
	// Don't go any further if the pin is un-usable or non-existant
	if (!pinValid(pin))
	{
		at_response_error();
		return;
	}
	
	if (*pPara++ != ',') { // skip ','
		at_response_error();
		return;
	}
	
	char mode = *pPara++;
	
	if ((mode == 'i') || (mode == 'I'))
	{
		if (pin == STATUS_LED_PIN)
		{
			wifi_status_led_uninstall();
		}
		PIN_FUNC_SELECT(pinMap[pin], pinFunction[pin]);
		GPIO_DIS_OUTPUT(pin);
		PIN_PULLUP_DIS(pinMap[pin]);
		os_sprintf(buffer, "%d=INPUT\r\n", pin);
	}
	else if ((mode == 'o') || (mode == 'O'))
	{
		if (pin == STATUS_LED_PIN)
		{
			wifi_status_led_uninstall();
		}
		PIN_FUNC_SELECT(pinMap[pin], pinFunction[pin]);
		GPIO_OUTPUT_SET(pin, 0);
		os_sprintf(buffer, "%d=OUTPUT\r\n", pin);
	}
	else if ((mode == 'p') || (mode == 'P'))
	{
		if (pin == STATUS_LED_PIN)
		{
			wifi_status_led_uninstall();
		}
		PIN_FUNC_SELECT(pinMap[pin], pinFunction[pin]);
		GPIO_DIS_OUTPUT(pin);
		PIN_PULLUP_EN(pinMap[pin]);
		os_sprintf(buffer, "%d=INPUT_PULLUP\r\n", pin);
	}
	else
	{
		at_response_error();
		return;
	}
	
	at_port_print(buffer);
	at_response_ok();
}

void ICACHE_FLASH_ATTR
at_testPinModeCmd(uint8_t id)
{
    at_port_print("AT+PINMODE=<pin>,<I/O/P>\r\n");
    at_response_ok();
}

// digitalWrite([pin], [value])
void ICACHE_FLASH_ATTR
at_setupWritePinCmd(uint8_t id, char *pPara)
{
    int result = 0, err = 0, flag = 0;
    uint8 buffer[32] = {0};
    pPara++; // skip '='

    //get the first parameter (uint8_t)
    // digit
    flag = at_get_next_int_dec(&pPara, &result, &err);

    // flag must be true because there are more parameters
    if ((flag == FALSE) && (result > 0) )
	{
        at_response_error();
        return;
    }
	
	uint8_t pin = result;
	
	// Don't go any further if the pin is un-usable or non-existant
	if (!pinValid(pin))
	{
		at_response_error();
		return;
	}
	
	if (*pPara++ != ',') { // skip ','
		at_response_error();
		return;
	}
	
	char value = *pPara++;
	
	if ((value == 'h') || (value == 'H') || (value == '1'))
	{
		GPIO_OUTPUT_SET(pin, 1);
		os_sprintf(buffer, "%d=HIGH\r\n", pin);
	}
	else if ((value == 'l') || (value == 'L') || (value == '0'))
	{
		GPIO_OUTPUT_SET(pin, 0);
		os_sprintf(buffer, "%d=LOW\r\n", pin);
	}
	else
	{
		at_response_error();
		return;
	}
	
	at_port_print(buffer);
	at_response_ok();
}

void ICACHE_FLASH_ATTR
at_testWritePinCmd(uint8_t id)
{
    at_port_print("AT+PINWRITE=<pin>,<H/L>");
    at_response_ok();
}

// digitalRead([pin])
void ICACHE_FLASH_ATTR
at_setupReadPinCmd(uint8_t id, char *pPara)
{
    int result = 0, err = 0, flag = 0;
    uint8 buffer[32] = {0};
    pPara++; // skip '='

    //get the first parameter (uint8_t)
    // digit
    flag = at_get_next_int_dec(&pPara, &result, &err);

    // flag must be true because there are more parameters
    //if ((flag == FALSE) && (result > 0) )
	if (err > 0)
	{
		at_port_print("Bad input");
        at_response_error();
        return;
    }
	
	uint8_t pin = result;
	
	// Don't go any further if the pin is un-usable or non-existant
	if (!pinValid(pin))
	{
		at_response_error();
		return;
	}
	
	uint8_t value = GPIO_INPUT_GET(pin);
	os_sprintf(buffer, "%d\r\n", value);
	
	at_port_print(buffer);
	at_response_ok();
}

void ICACHE_FLASH_ATTR
at_testReadPinCmd(uint8_t id)
{
    at_port_print("AT+PINREAD=<pin>");
    at_response_ok();
}

at_funcationType at_duinoShield_cmd[] = {
	//   CMD String         CMD String Size         CMD Test CB   CMD Query CB  CMD Setup CB   CMD Execute CB
	{  SET_PINMODE_CMD,   SET_PINMODE_CMD_SIZE,  at_testPinModeCmd, NULL,  at_setupPinModeCmd,  NULL},
	{    WRITE_PIN_CMD,     WRITE_PIN_CMD_SIZE, at_testWritePinCmd, NULL, at_setupWritePinCmd,  NULL},
	{     READ_PIN_CMD,      READ_PIN_CMD_SIZE,  at_testReadPinCmd, NULL,  at_setupReadPinCmd,  NULL}
};

void ICACHE_FLASH_ATTR
shield_init()
{
	wifi_status_led_install(STATUS_LED_PIN, STATUS_LED_MUX, STATUS_LED_FUNC);
	
	uart_init(DEFAULT_BAUD_RATE, DEFAULT_BAUD_RATE);
	
	os_memset(resetCommand, MAX_RESET_STR_LEGNTH + 1, 0);
	
    at_cmd_array_regist(&at_duinoShield_cmd[0], sizeof(at_duinoShield_cmd)/sizeof(at_duinoShield_cmd[0]));
}