#ifndef KRA_DALLASTEMP
#define KRA_DALLASTEMP

#include <Arduino.h>
#include <OneWireNg_CurrentPlatform.h>

/* DS therms commands */
#define CMD_CONVERT_T           0x44
#define CMD_COPY_SCRATCHPAD     0x48
#define CMD_WRITE_SCRATCHPAD    0x4E
#define CMD_RECALL_EEPROM       0xB8
#define CMD_READ_POW_SUPPLY     0xB4
#define CMD_READ_SCRATCHPAD     0xBE

/* supported DS therms families */
#define DS18S20     0x10
#define DS1822      0x22
#define DS18B20     0x28
#define DS1825      0x3B
#define DS28EA00    0x42

#define ARRSZ(t) (sizeof(t)/sizeof((t)[0]))

static struct {
    uint8_t code;
    const char *name;
} DSTH_CODES[] = {
    { DS18S20, "DS18S20" },
    { DS1822, "DS1822" },
    { DS18B20, "DS18B20" },
    { DS1825, "DS1825" },
    { DS28EA00,"DS28EA00" }
};

const char *dsthName(const OneWireNg::Id& id);

class KRA_DallasTemp 
{
    public:
        
        static bool printId(const OneWireNg::Id& id);
    private:
        
};

#endif