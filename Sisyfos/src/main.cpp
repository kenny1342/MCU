/**
 * Sisyfos
 * 
 * Become USB HID (keyboard), send control char keystrokes to the computer at intervals 
 * in order to prevent hibernation/screensaver activation and keep VPN/network up.
 *  
 * Also change vendor id in lib .platformio/packages/framework-arduino-avr-digistump/libraries/DigisparkKeyboard/usbconfig.h
 * See https://devicehunt.com/all-usb-vendors
 * #define USB_CFG_VENDOR_ID 0xac, 0x05 // well known trusted tech company
 * #define USB_CFG_DEVICE_ID 0x27, 0x22
 * #define USB_CFG_VENDOR_NAME     'K','R','A','t','e','c','h',' ','L','a','b','s'
 * #define USB_CFG_VENDOR_NAME_LEN 12
 * #define USB_CFG_DEVICE_NAME     'S','i','s','y','f','o','s'
 * #define USB_CFG_DEVICE_NAME_LEN 7
 *
 * v1.0 (2021-03-25) - initial version
 * v1.1 (2021-04-01) - fix LED board model B. add post-build script.
 * 
 * Tested on ATtiny85 (Digispark board, http://digistump.com/products/1)
 *
 * Ken-Roger Andersen <ken.roger@gmail.com>
 **/

#include <Arduino.h>
#include "DigiKeyboard.h"
#include <Timemark.h>

#define LED_PIN_A   1 // LED on Digispark board Model A
#define LED_PIN_B   0 // LED on Digispark board Model B

#ifndef KEY_ESC 
#define KEY_ESC     41  
#endif
#ifndef KEY_PRT_SCR
#define KEY_PRT_SCR 70
#endif
#define ON  0x1
#define OFF 0x0
#define SET_LED(STATE) digitalWrite(LED_PIN_A, STATE); digitalWrite(LED_PIN_B, STATE);

Timemark tm_keypress(40000);


void setup() {    
  uint8_t x = 0;
  pinMode(LED_PIN_A, OUTPUT); 
  pinMode(LED_PIN_B, OUTPUT); 

  DigiKeyboard.update();
  while(x < 30) {
    x++;
    SET_LED(ON);
    DigiKeyboard.delay(40);
    SET_LED(OFF);
    DigiKeyboard.delay(40);
  }

  DigiKeyboard.update();
  DigiKeyboard.delay(500);
  DigiKeyboard.sendKeyStroke(0, MOD_CONTROL_RIGHT);  
  tm_keypress.start();
}

void loop() {

  uint8_t x = 0;
  
  DigiKeyboard.update();

  if(tm_keypress.expired()) {

    // Flash LED to indicate keypress
    x = 0;
    while(x < 3) {
      x++;
      SET_LED(ON);
      DigiKeyboard.delay(50);
      SET_LED(OFF);
      DigiKeyboard.delay(50);
    }

    DigiKeyboard.sendKeyStroke(0);
    SET_LED(ON);
    DigiKeyboard.sendKeyStroke(0, MOD_CONTROL_RIGHT);
    DigiKeyboard.delay(300);    
    SET_LED(OFF);
    DigiKeyboard.delay(300);
  }

  DigiKeyboard.delay(10);
}