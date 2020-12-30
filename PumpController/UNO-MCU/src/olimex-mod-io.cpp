#include "Arduino.h"
#include <main.h>

#ifdef SW_I2C
//#include <SoftWire.h>
//#include <AsyncDelay.h>
#include <SlowSoftI2CMaster.h>
#else
#include "Wire.h"
#endif

#include "olimex-mod-io.h"

#ifdef SW_I2C
//SoftWire I2C(PIN_SDA, PIN_SCL);
SlowSoftI2CMaster I2C(PIN_SDA, PIN_SCL, true);
// These buffers must be at least as large as the largest read or write you perform.
//char swTxBuffer[16];
//char swRxBuffer[16];
#else
// TODO: I2C = Wire....
#endif

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
}

void ModIOInitBoard() {
#ifdef SW_I2C
  if (!I2C.i2c_init()) 
    Serial.println(F("Initialization error. SDA or SCL are low"));
  else
    Serial.println(F("...done"));
  /*I2C.setTxBuffer(swTxBuffer, sizeof(swTxBuffer));
  I2C.setRxBuffer(swRxBuffer, sizeof(swRxBuffer));
  I2C.setDelay_us(5);
  I2C.setTimeout(1000);
  I2C.begin();
  */
#endif
}

/*
 Set relays according to external variable relayState
 */
uint8_t ModIOUpdateRelays()
{
  //uint8_t ack;
  
  Serial.print("MOD-IO: "); Serial.print(CONF_I2C_ID_MODIO_BOARD, HEX); Serial.print("="); Serial.println(relayState, BIN);

    if (I2C.i2c_start(CONF_I2C_ID_MODIO_BOARD | I2C_WRITE)) {
        
        I2C.i2c_write(0x10);
        I2C.i2c_write(relayState);
        I2C.i2c_rep_start((CONF_I2C_ID_MODIO_BOARD<<1)|I2C_READ);
        byte val = I2C.i2c_read(true);
        Serial.println(val);
        I2C.i2c_stop();
    } else {
        Serial.println(F("i2c_start failed!"));
    }
  /*
  I2C.beginTransmission(CONF_I2C_ID_MODIO_BOARD); // 0x58 = board i2c ID
  //Serial.println("MOD-IO write 0x10"); 
  I2C.write(0x10); // cmd to set relays
  I2C.write(relayState);                       
  */
   //Serial.println(F("MOD-IO endTransmission")); 
  //return (uint8_t) I2C.endTransmission(1); // 0=do not send stop (no-block)
  return 0;
}

uint8_t ModIOSetRelay(unsigned char relay, unsigned char mode) {
    
    // Setting the nth bit to either 1 or 0 can be achieved with the following:
    // number ^= (-x ^ number) & (1 << n);
    Serial.print(F("relay ")); Serial.print(relay); Serial.print(F("=")); Serial.println(mode);

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