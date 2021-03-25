# Sisyfos

A simple USB HID that prevent hibernation/keep computer awake

## Description

Act as USB HID (keyboard), send control char keystrokes to the computer at intervals
in order to prevent hibernation/screensaver activation and keep VPN/network up.

## Getting Started

Method 1: Compile and program from source using PlatformIO or Arduino IDE

Method 2: Burn HEX file in release/ folder to board using [Xloader](https://www.hobbytronics.co.uk/arduino-xloader) or Arduino IDE

Tested on ATtiny85 (Digispark board, <http://digistump.com/products/1>)

## Authors

Ken-Roger Andersen <ken.roger@gmail.com>

## Version History

* 1.0 (2021-03-25)
  * Initial Release
