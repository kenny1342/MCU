#pragma once
#ifndef olimexmodio__h
#define olimexmodio__h 

#include "Arduino.h"
#include <main.h>

#ifndef PIN_ModIO_Reset
    #define PIN_ModIO_Reset  5
#endif

#ifndef CONF_I2C_ID_MODIO_BOARD
    #define CONF_I2C_ID_MODIO_BOARD  0x58
#endif

#define MODIO_ACK_OK    0

void ModIO_Reset(void);
void ModIO_Init();
uint8_t ModIO_Update();
uint8_t ModIO_SetRelayState(unsigned char relay, unsigned char mode);
uint8_t ModIO_GetRelayState(unsigned char relay);

#endif