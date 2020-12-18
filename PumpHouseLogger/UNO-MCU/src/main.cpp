#include <Arduino.h>
#include <SoftwareSerial.h>

SoftwareSerial mySerial(2, 3); // RX, TX

#define  LM335_pin  0

int  Kelvin, Celsius;
char message1[] = "Temp =  00.0 C";
char message2[] =      "=  00.0 K";


void setup()
{
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Native USB only
  }


  Serial.println("Goodnight moon!");

  // set the data rate for the SoftwareSerial port
  mySerial.begin(115200);
  mySerial.println("Hello, world?");
}

void loop() // run over and over
{
  /*
  if (mySerial.available())
    Serial.write(mySerial.read());
  if (Serial.available())
    mySerial.write(Serial.read());
*/

    Kelvin = analogRead(LM335_pin) * 0.489;      // Read analog voltage and convert it to Kelvin (0.489 = 500/1023)
  Celsius = Kelvin - 273;                      // Convert Kelvin to degree Celsius
  if(Celsius < 0){
    Celsius = abs(Celsius);                    // Absolute value
    message1[7] = '-';                         // Put minus '-' sign
  }
  else
    message1[7]  = ' ';                        // Put space ' '
  if (Celsius > 99)
    message1[7]  = '1';                        // Put 1 (of hundred)
  message1[8]  = (Celsius / 10) % 10  + 48;
  message1[9]  =  Celsius % 10  + 48;
  message1[12] =  ' '; ///223;                         // Put degree symbol
  message2[2]  = (Kelvin / 100) % 10 + 48;
  message2[3]  = (Kelvin / 10) % 10 + 48;
  message2[4]  = Kelvin % 10 + 48;

Serial.println(message1);
mySerial.println(message1);
      delay(500);
      Serial.println(message2);
      mySerial.println(message2);
      delay(1000);
}
