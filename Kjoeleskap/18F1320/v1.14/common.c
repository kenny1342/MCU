/**
* Read X samples from ADC, average them and return result
* If samples=1, just one reading is done (rearly used)
**/
unsigned int16 getAvgADC(unsigned int8 channel, unsigned int8 samples)
{
   unsigned int16 temp_adc_value=0;
   unsigned int8 x;
   
   if(samples == 0) samples = 20;
   
   for (x=0;x<samples;x++)
   {
      set_adc_channel ( channel );
      delay_us(50);
      temp_adc_value += read_adc();
   }

   temp_adc_value /= samples;

   if (temp_adc_value < 1) temp_adc_value=1; // never divide by zero:

   return temp_adc_value;
}

/**
* Set all bits in given range in data EEPROM
*/
void fillEEPROM(unsigned int8 byte_start, unsigned int8 byte_end)
{
    unsigned int8 x;
   for(x=byte_start;x<byte_end;x++)
      write_eeprom(x, 0xFF);
}

/**
* Clear all bits in given range in data EEPROM
*/
void clearEEPROM(unsigned int8 byte_start, unsigned int8 byte_end)
{
   unsigned int8 x;
   for(x=byte_start;x<byte_end;x++)
      write_eeprom(x, 0x00);
}

/*
Generic EPROM save/retrieve routines, works good for hole structs etc
Struct Example:

struct PID {
   int8 test;
} pid_data;
eeprom_put(&pid_data,sizeof(struct PID),0x00); 
eeprom_get(&pid_data,sizeof(struct PID),0x00); 
*/

/**
* Write num bytes from array/stuct pointer to EEPROM, starting at addr
*/
void eeprom_put(int *ptr,int num,int addr)
{
   unsigned int8 count;
   for (count=0;count<num;count++) {
      write_eeprom(addr+count,*ptr++);
   }
} 

/**
* Read from EEPROM starting at addr, into data to array/stuct pointer 
*/
void eeprom_get(int *ptr,int num,int addr)
{
   unsigned int8 count;
   for (count=0;count<num;count++) {
      *ptr++=read_eeprom(addr+count);
   }
}
