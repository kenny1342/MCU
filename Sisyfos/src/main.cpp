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
 * 
 * Tested on ATtiny85 (Digispark board, http://digistump.com/products/1)
 *
 * Ken-Roger Andersen <ken.roger@gmail.com>
 **/

#include <Arduino.h>
#include <DigiKeyboard.h>
#include <Timemark.h>

#define LED_PIN   1 // LED on Digispark board Model A

#define KEY_ESC     41
#define KEY_BACKSPACE 42
#define KEY_TAB     43
#define KEY_PRT_SCR 70
#define KEY_DELETE  76
#define KEY_ARROW_RIGHT 0x4F
#define KEY_ARROW_DOWN  0x51
#define KEY_ARROW_UP    0x52

Timemark tm_keypress(40000);


void setup() {    
  uint8_t x = 0;
  pinMode(LED_PIN, OUTPUT); 

  DigiKeyboard.update();
  while(x < 10) {
    x++;
    digitalWrite(LED_PIN, HIGH);
    DigiKeyboard.delay(40);
    digitalWrite(LED_PIN, LOW);
    DigiKeyboard.delay(40);
  }

  DigiKeyboard.update();
  DigiKeyboard.delay(1000);
  DigiKeyboard.sendKeyStroke(0);  
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
      digitalWrite(LED_PIN, HIGH);
      DigiKeyboard.delay(50);
      digitalWrite(LED_PIN, LOW);
      DigiKeyboard.delay(50);
    }

    DigiKeyboard.sendKeyStroke(0);
    digitalWrite(LED_PIN, HIGH);
    DigiKeyboard.sendKeyStroke(0, MOD_CONTROL_RIGHT);
    DigiKeyboard.delay(300);    
    digitalWrite(LED_PIN, LOW);
    DigiKeyboard.delay(300);
  }

  DigiKeyboard.delay(10);
}