#include "Arduino.h"
#include <main.h>
#include "Wire.h"
#include "olimex-mod-io.h"


uint8_t relayState = 0x00;

/***** MOD-IO RELAY BOARD *****/

// Instead of Macro
void RESET_MODIO(void) {  
    Serial.println(F("RST MODIO...")); 
    pinMode(PIN_RESET_MODIO, OUTPUT); 
    delay(5); 
    digitalWrite(PIN_RESET_MODIO, LOW); 
    delay(10); 
    digitalWrite(PIN_RESET_MODIO, HIGH); 
    pinMode(PIN_RESET_MODIO, INPUT); 
    delay(100);
}

void ModIOInitBoard() {

}

/*
 Set relays according to external variable relayState
 */
uint8_t ModIOUpdateRelays()
{
  //uint8_t ack;
  
  //Serial.print("MOD-IO: "); Serial.print(CONF_I2C_ID_MODIO_BOARD, HEX); Serial.print("="); Serial.println(relayState, BIN);
  
  Wire.beginTransmission(CONF_I2C_ID_MODIO_BOARD); // 0x58 = board i2c ID
  //Serial.println("MOD-IO write 0x10"); 
  Wire.write(0x10); // cmd to set relays
  Wire.write(relayState);                       
  return (uint8_t) Wire.endTransmission(1); // 0=do not send stop (no-block)
}

uint8_t ModIOSetRelay(unsigned char relay, unsigned char mode) {
    
    // Setting the nth bit to either 1 or 0 can be achieved with the following:
    // number ^= (-x ^ number) & (1 << n);
    //Serial.print(F("relay ")); Serial.print(relay); Serial.print(F("=")); Serial.println(mode);

    switch(mode) //0=OFF,1=ON,2=toggle
    {
        case 0: relayState &= ~(1 << relay); break; // Use the bitwise AND operator (&) to clear a bit.
        case 1: relayState |= 1 << relay; break;    // Use the bitwise OR operator (|) to set a bit.
        case 2: relayState ^= 1 << relay; break;    // The XOR operator (^) can be used to toggle a bit.
        default: relayState ^= 1 << relay;
    }
    
    //if(ModIOGetRelayStatus(relay) != mode)
    return ModIOUpdateRelays();

}

uint8_t ModIOGetRelayStatus(unsigned char relay) {
    //To check a bit, shift the number x to the right, then bitwise AND it:
    //bit = (number >> x) & 1;

     return (uint8_t) (relayState >> relay) & 1;

     //return 0xff;
}