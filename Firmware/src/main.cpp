#include <Adafruit_GFX.h> //graphics library
#include <Adafruit_ST7789.h> //driver for ST7789 screen
#include <time.h>
#include <SPI.h>
#include <Ticker.h>

//Defines the pins
#define TFT_SCLK 21 // SCL on display
#define TFT_MOSI 20 // SDA on display
#define TFT_RST 10
#define TFT_DC 8
#define TFT_CS 7
#define TFT_BL 6
//#define BUZZER 5
#define BUTTON1 0
//#define BUTTON2 1
//#define BUTTON3 2
//#define BUTTON4 3
//#define BUTTON5 4

bool pastBM1 = HIGH; //past button mode 1

class MyST7789 : public Adafruit_ST7789 {
    public:
        using Adafruit_ST7789::Adafruit_ST7789;

    void setOffsets(uint8_t x, uint8_t y) {
        _xstart = x;
        _ystart = y;
    }
};

MyST7789 tft(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

volatile int seconds = 0;
volatile int minutes = 0;
volatile int hours = 0;
int lastSecond = -1;
int lastMinute = -1;
int clockMode = 0;

Ticker clockTicker;

void clockTick() {
  seconds++; //add a second to clock
  if (seconds >= 60) {
    seconds = 0;
    minutes++;
    if (minutes >= 60) {
      minutes = 0;
      hours++;
      if (hours >= 24) {
        hours = 0;
      }
    }
  }
}

void startUpMessage() {
    tft.print("ouu shii");
}

void clearScreen() {
    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(10,10);
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, LOW);

    pinMode(BUTTON1, INPUT_PULLUP);


    tft.init(76, 284); //display size
    tft.setOffsets(82, 18);
    tft.invertDisplay(false);
    tft.setRotation(1);

    tft.fillScreen(ST77XX_BLACK);

    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(5);
    tft.setCursor(10,10);
    tft.print("CLOCK");

    clockTicker.attach(1.0, clockTick);
}

void loop() {
    if (clockMode == 0) {
            tft.setTextSize(8);
            if (lastMinute != minutes) { //this if loop lets the screen not try to reprint the time when it doesn't need to
            clearScreen();
            tft.printf("%02d:%02d\n", hours, minutes);
            lastMinute = minutes;
        }
    } else if (clockMode == 1) {
            tft.setTextSize(5);
            if (lastSecond != seconds) { //this if loop lets the screen not try to reprint the time when it doesn't need to
            clearScreen();
            tft.printf("%02d:%02d:%02d\n", hours, minutes, seconds);
            lastSecond = seconds;
        }
    }

    bool currentModeBtnState = digitalRead(BUTTON1);
    if (currentModeBtnState == LOW && pastBM1 == HIGH) {
        
        Serial.println("Button was pressed!");
        delay(200);
    }

    pastBM1 = currentModeBtnState;
}