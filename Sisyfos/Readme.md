# Sisyfos

A simple USB HID that prevent hibernation/keep computer awake

## Description

Act as USB HID (keyboard), send control char keystrokes to the computer at intervals
in order to prevent hibernation/screensaver activation and keep VPN/network up.

## Pics

<img src="https://raw.githubusercontent.com/kenny1342/MCU/master/Sisyfos/sisyfos_hw1.jpg" width="480" height="480">

<img src="https://raw.githubusercontent.com/kenny1342/MCU/master/Sisyfos/sisyfos_hw2.jpg" width="480" height="480">

## USB identification/enumeration

<img src="https://raw.githubusercontent.com/kenny1342/MCU/master/Sisyfos/usb_enum.jpg">

## Getting Started 

### Digispark boards (ATtiny85 chip with Micronucleus USB bootloader pre-installed)

These boards comes with a pre-installed bootloader that makes it easy to upload firmware over USB, without the need for an external ISP/programmer.

After you plug in the board to the USB port, the **bootloader will accept firmware uploads for 5 secs**, after that it switches and runs the installed firmware.

Upload HEX firmware file in release/ folder (https://github.com/kenny1342/MCU/tree/master/Sisyfos/release) to board using Micronucleus

- Install [Micronucleus](https://github.com/micronucleus/micronucleus/releases)
- Install USB driver as instructed (https://github.com/micronucleus/micronucleus/tree/master/windows_driver_installer)
- Write the HEX file to board , f.ex: run C:\Users\Administrator\Desktop\i686-mingw32>micronucleus.exe --run sisyfos_1.0_digispark.hex and insert board into USB port

<img src="https://raw.githubusercontent.com/kenny1342/MCU/master/Sisyfos/hexupload.jpg">

Tested on ATtiny85 (Digispark board, <http://digistump.com/products/1>)

## Authors

Ken-Roger Andersen <ken.roger@gmail.com>

## Version History

* 1.0 (2021-03-25)
  * Initial Release
* 1.1 (2021-04-01)
  * fix LED board model B. add post-build script.
  
## External links

https://www.conrad.com/p/joy-it-arduino-expansion-board-digispark-microcontroller-1503743?WT.srch=1&vat=true

http://digistump.com/products/1

https://www.banggood.com/Digispark-Kickstarter-Micro-Usb-Development-Board-For-ATTINY85-p-1038088.html?cur_warehouse=CN&rmmds=search

ATmega32u4 version in a stick:
https://www.banggood.com/LILYGO-USB-Microcontroller-ATMEGA32U4-Development-Board-Virtual-Keyboard-5V-DC-16MHz-5-Channel-p-1356220.html?rmmds=detail-left-hotproducts&cur_warehouse=CN