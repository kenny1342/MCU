#define IS_LEAP(year) (year%4 == 0)
unsigned int16 getAvgADC(unsigned int8 channel, unsigned int8 samples);
void fillEEPROM(unsigned int8 byte_start, unsigned int8 byte_end);
void clearEEPROM(unsigned int8 byte_start, unsigned int8 byte_end);
void eeprom_put(int *ptr,int num,int addr);
void eeprom_get(int *ptr,int num,int addr);
