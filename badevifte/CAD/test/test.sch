EESchema Schematic File Version 1
LIBS:power,device,transistors,conn,linear,regul,74xx,cmos4000,adc-dac,memory,xilinx,special,microcontrollers,dsp,microchip,analog_switches,motorola,texas,intel,audio,interface,digital-audio,philips,display,cypress,siliconi,contrib,valves
EELAYER 43  0
EELAYER END
$Descr A4 11700 8267
Sheet 1 1
Title ""
Date "27 apr 2008"
Rev ""
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
Wire Wire Line
	6600 2100 6550 2100
Wire Wire Line
	6550 2100 6550 2050
Wire Wire Line
	6550 2050 4950 2050
Wire Wire Line
	6200 1800 6200 1750
Wire Wire Line
	6200 1750 5350 1750
Wire Wire Line
	4550 1750 4550 2300
Wire Wire Line
	4550 2300 7000 2300
Wire Wire Line
	7000 2300 7000 1800
$Comp
L LM7812 U2
U 1 1 48148E76
P 6600 1850
F 0 "U2" H 6750 1654 60  0000 C C
F 1 "LM7812" H 6600 2050 60  0000 C C
	1    6600 1850
	1    0    0    -1  
$EndComp
$Comp
L LM7812 U1
U 1 1 48148E6F
P 4950 1800
F 0 "U1" H 5100 1604 60  0000 C C
F 1 "LM7812" H 4950 2000 60  0000 C C
	1    4950 1800
	1    0    0    -1  
$EndComp
$EndSCHEMATC
