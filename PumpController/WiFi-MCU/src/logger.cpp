#include <Arduino.h>
#include <TFT_eSPI.h>
#include <logger.h>


Logger::Logger(TFT_eSPI * _tft) { // TFT_eSPI tft = TFT_eSPI(135, 240); // Invoke custom library
  this->tft = _tft;
}

void Logger::print(char *str) {
    this->func_nl = false;
    this->println(str);
}

void Logger::println(char *str) {
    this->println(str, true);
}

void Logger::print(const char *str) {
    this->func_nl = false;
    this->println(str);
}

void Logger::println(const char *str) {
    char * s = new char[strlen(str)+1];
    strcpy(s, str);
    this->println(s, true);
}

void Logger::print(const __FlashStringHelper *str) {
    this->func_nl = false;
    this->println(str);
}

void Logger::println(const __FlashStringHelper *str) {
    char buffer[160];

    strcpy_P(buffer , (const prog_char*) str);
    this->println(buffer, true);
}

/**
 * Insert '\n' every 20 char, and print on TFT display + serial
 */
//void Logger::println(char *str, bool clear_lcd) {
/**
 * print on TFT display + serial
 */
void Logger::println(char *line, bool clear_lcd) {

    this->tft->setCursor(0, 0);
    if(clear_lcd) {
        this->tft->fillScreen(TFT_BLACK);
    }

    /*
    char line[200] = {0};

    size_t l = strlen(str);
    int i = 0;
    int n = 0;
    for(int x=0; x<l; x++) {

    if(str[x] == '\0') break;

        line[x+n] = str[x];

        if(++i == 20) {
            if(x+n < sizeof(line)-1) {
            i = 0;
            n++;
            line[x+n] = '\n';
            }
        }
    }

    size_t length = strlen(line);

    if(line[length] != '\0') line[length] = '\0';
*/
    //Serial.println("line START");
    if(this->func_nl) {
        Serial.println(line);
    } else {
        Serial.print(line);
    }
    
    this->func_nl = true;
    //Serial.println("line END");
    this->tft->println(line);
}

