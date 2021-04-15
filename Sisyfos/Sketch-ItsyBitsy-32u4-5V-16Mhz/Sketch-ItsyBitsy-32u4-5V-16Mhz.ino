
// minimal demo (tested with Arduino IDE and Addafruit ItsyBitsy 32u4 5V 16Mhz)

#include "Keyboard.h"
const int LED = 13;

void setup() {
  // make the pushButton pin an input:
  pinMode(LED, OUTPUT);
  // initialize control over the keyboard:
  Keyboard.begin();
}

void loop() {

  digitalWrite(LED, 1);
  Keyboard.write(KEY_RIGHT_CTRL);
  delay(200);
  digitalWrite(LED, 0);
  delay(10000);
}