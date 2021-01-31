#include <Arduino.h>
#include <OneWireNg_CurrentPlatform.h>
#include "KRA_DallasTemp.h"


/* returns NULL if not supported */
 const char *dsthName(const OneWireNg::Id& id)
{
    for (size_t i=0; i < ARRSZ(DSTH_CODES); i++) {
        if (id[0] == DSTH_CODES[i].code)
            return DSTH_CODES[i].name;
    }
    return NULL;
}

/* returns false if not supported */
 bool KRA_DallasTemp::printId(const OneWireNg::Id& id)
{
    const char *name = dsthName(id);

    Serial.print(id[0], HEX);
    for (size_t i=1; i < sizeof(OneWireNg::Id); i++) {
        Serial.print(':');
        Serial.print(id[i], HEX);
    }
    if (name) {
        Serial.print(" -> ");
        Serial.print(name);
    }
    Serial.println();

    return (name ? true : false);
}
