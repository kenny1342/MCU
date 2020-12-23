# MCU / Electronic projects
> Here I will add some old and new electronic projects I have played with over the years, mostly the ones based around microcontrollers

## Current projects

### PumpController (Arduino core) (2020->)
This is a module in my home automation system, it's responsible for monitoring and control of water pressure, temperature and (to come) mechanical parameters of the water pump for my house
* ESP8266, Atmega328P / C/C++


## Old projects

### Kjøleskap (Fridge/Freezer controller) (PIC) (2010)
My fridge died because the capacitor in the C/R power supply failed, killing the PIC MCU on the board. I decided it was worth trying to repair. \
I replaced the powersupply with a safer external, transformer/LM7805 based, and replaced the original MCU (an obsolete PIC chip) with a pin-compitable 16F628. \
This is the firmware I wrote, trying to replicate the functionality in the old to operate compressor relays, switches, temp sensors and LEDs. \
I also added a RS232 data logger, connected to a webserver and showed the fridge/freezer status (temperatures etc) online, my early take on IoT. I had this running for 9 years, then I bought a new appliance after refurbishing my kitchen.
* PIC16F628A / C++ (PICC)

### Badevifte (Bathroom fan controller) (PIC) (2007)
One of my first usable projects, temp/humidity controlled bathroom fan. Badly coded but a great learning experience.
* PIC16F876 / C++ (PICC)

