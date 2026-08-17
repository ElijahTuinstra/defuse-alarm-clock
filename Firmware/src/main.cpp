#include <Adafruit_GFX.h> //graphics library
#include <Adafruit_ST7789.h> //driver for ST7789 screen
#include <SPI.h>

//Defines the pins
#define TFT_SCLK 21 // SCL on display
#define TFT_MOSI 20 // SDA on display
#define TFT_RST 10
#define TFT_DC 8
#define TFT_CS 5
#define TFT_BL 6

class MyST7789 : public Adafruit_ST7789 {
    public:
        using Adafruit_ST7789::Adafruit_ST7789;

    void setOffsets(uint8_t x, uint8_t y) {
        _xstart = x;
        _ystart = y;
    }
};

//MyST7789 tft(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);
MyST7789 tft(TFT_CS, TFT_DC, TFT_RST);

void setup() {
    Serial.begin(115200);

    neopixelWrite(7, 10, 10, 10); //sets the stupid LED on the board to low brightness, otherwise it is BLINDING

    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, LOW);

    SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS); 

    tft.init(76, 284); //display size
    tft.setOffsets(82, 18);
    tft.invertDisplay(false);
    tft.setRotation(1);

    tft.fillScreen(ST77XX_BLACK);

    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(6);
    tft.setCursor(10,10);
    tft.print(42);
}

void loop() {

}