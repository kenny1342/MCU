#pragma once
#ifndef olimexmodio__h
#define olimexmodio__h 

#include "Arduino.h"
#include <main.h>

#ifndef PIN_RESET_MODIO
    #define PIN_RESET_MODIO  5
#endif

#ifndef CONF_I2C_ID_MODIO_BOARD
    #define CONF_I2C_ID_MODIO_BOARD  0x58
#endif

#define MODIO_ACK_OK    0

//extern uint8_t relayState;


void RESET_MODIO(void);
void ModIOInitBoard();
uint8_t ModIOUpdateRelays();
uint8_t ModIOSetRelay(unsigned char relay, unsigned char mode);
uint8_t ModIOGetRelayStatus(unsigned char relay);

#endif