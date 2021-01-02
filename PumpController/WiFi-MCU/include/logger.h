#include <Arduino.h>
#include <TFT_eSPI.h>


class Logger {
    private:
        bool newline = true;
        //char *prependstr(char *str, const char *pstr);
    public:
    //static const char*  _configfile;// = "/config.json";
    TFT_eSPI *tft;
    Logger(TFT_eSPI * _tft);

    void print(char *str);
    void println(char *str);
    void print(const char *str);
    void println(const char *str);
    void print(const __FlashStringHelper *str);
    void println(const __FlashStringHelper *str);
    void print(char *str, bool clear_lcd);
    void println(char *str, bool clear_lcd);
    //void TFTprintln(const char *str);


};
