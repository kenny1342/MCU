
// minimal demo (tested with Arduino IDE and Adafruit ItsyBitsy 32u4 5V 16Mhz)
// https://www.arduino.cc/reference/en/language/functions/usb/keyboard/keyboardmodifiers/

#include "Keyboard.h"
const int LED = 13;

void setup() {
  uint8_t x = 0;
  // make the pushButton pin an input:
  pinMode(LED, OUTPUT);
  for(x=0; x<20; x++) {
    digitalWrite(LED, 1);  
    delay(100);
    digitalWrite(LED, 0);
    delay(100);
  }
  // initialize control over the keyboard:
  Keyboard.begin();
}

void loop() {

  digitalWrite(LED, 1);
  Keyboard.write(KEY_F24);
  delay(200);
  digitalWrite(LED, 0);
  delay(1000);
}
