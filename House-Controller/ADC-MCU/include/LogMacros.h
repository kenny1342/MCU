// LogMacros.h

/* Example of use:
   #define LOGGER          // <-----<<<< this line must appear before the include line
   #include <LogMacros.h>
*/

#ifdef DEBUG
//#define DPRINT(args...)  Serial.print(args)             //OR use the following syntax:
#define SERIALBEGIN(...)   Serial.begin(__VA_ARGS__)
#define LOGERR_PRINT(...)       Serial.print("ERROR: "); Serial.print(__VA_ARGS__)
#define LOGERR_PRINTLN(...)     Serial.print("ERROR: "); Serial.println(__VA_ARGS__)
#define LOGERR_PRINTF(...)      Serial.print("ERROR: "); Serial.print(F(__VA_ARGS__))
#define LOGERR_PRINTLNF(...)    Serial.print("ERROR: "); Serial.println(F(__VA_ARGS__)) //Printing text using the F macro
 
#else
#define LOGERR_PRINT(...)        //blank line
#define LOGERR_PRINTLN(...)      //blank line
#define LOGERR_PRINTF(...)       //blank line
#define LOGERR_PRINTLNF(...)     //blank line
 
#endif
