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
#define BUZZER 0
#define BUTTON1 1
#define BUTTON2 2
#define BUTTON3 3
#define BUTTON4 4
#define BUTTON5 5

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
int lastHour = -1;
int displayMode = 0;
bool setupComplete = false;
bool timeSet = false;
const char* timeUnits[] = {"Hours", "Minutes", "Seconds"};
int timeUnitsListSize = sizeof(timeUnits) / sizeof(timeUnits[0]);
enum Mode { HOURS, MINUTES, SECONDS };
Mode clockMode = HOURS;

struct SetupResult {
  int updatedValue;
  bool unitFinished;
};


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

struct Button {
    uint8_t pin;
    bool lastState;
    const char* name;
};

Button clockButtons[] = {
    {BUTTON1, HIGH, "Button 1"},
    {BUTTON2, HIGH, "Button 2"},
    {BUTTON3, HIGH, "Button 3"},
    {BUTTON4, HIGH, "Button 4"},
    {BUTTON5, HIGH, "Button 5"}
};

const int NUM_BUTTONS = sizeof(clockButtons) / sizeof(clockButtons[0]);


bool buttonPressed(Button& button) {
    bool currentState = digitalRead(button.pin);
    bool triggered = false;

    if (currentState == LOW && button.lastState == HIGH) {
        triggered = true;
        delay(50);
    }

    button.lastState = currentState;
    return triggered;
}

bool anyButtonPressed() {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (buttonPressed(clockButtons[i])) {
      return true;
    }
  }
  return false;
}

Button* whichButtonPressed() {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (buttonPressed(clockButtons[i])) {
      return &clockButtons[i];
    }
  } 
  return NULL;
}

void resetScreen() {
    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(10,10);
}

void printHourMinute() {
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK); 
    tft.setTextSize(8);
    tft.printf("%02d:%02d\n", hours, minutes);
}

void printHourMinuteSecond() {
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setTextSize(5);
    tft.printf("%02d:%02d:%02d\n", hours, minutes, seconds);
}

void displayClock() {
    if (displayMode == 0) {
        if (lastMinute != minutes) { //this if function lets the screen not try to reprint the time when it doesn't need to
            tft.setCursor(10, 10);
            printHourMinute();
            lastMinute = minutes;
        }
    } else if (displayMode == 1) {
        if (lastSecond != seconds) { //this if function lets the screen not try to reprint the time when it doesn't need to
            tft.setCursor(10, 10);
            printHourMinuteSecond();
            lastSecond = seconds;
        }
    }
}

void changeClockMode() {
    resetScreen();
    if (displayMode == 0) {
        displayMode = 1;
        resetScreen();
        printHourMinuteSecond();
    } else if (displayMode == 1) {
        displayMode = 0;
        resetScreen();
        printHourMinute();
    }
}

SetupResult chooseSetUnitOfTime(int valueOfUnitOfMeasurement, const char* labelUnitOfMeasurement, Mode targetedMode) {
    SetupResult result;

    resetScreen();
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.printf("Set %s. 1 to decrease, 2 to increase, 5 to confirm.", labelUnitOfMeasurement);
    tft.println(); 
    tft.println();    
    int cursorX = tft.getCursorX();
    int cursorY = tft.getCursorY();


    int maxValue = 60;
    int lastValue = -1;

    int localValue = valueOfUnitOfMeasurement;

    switch(targetedMode) {
        case HOURS:
            maxValue = 24;
            lastValue = hours;
            break;

        case MINUTES:
            maxValue = 60;
            lastValue = minutes;
            break;

        case SECONDS:
            maxValue = 60;
            lastValue = seconds;
            break;
    }

    lastValue = -1;

    bool unitSet = false;
    while(!unitSet){
        if(localValue != lastValue) {
            tft.setCursor(cursorX, cursorY);
            tft.setTextSize(3);
            tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
            tft.printf("%s:%02d    ", labelUnitOfMeasurement, localValue);
            lastValue = localValue;
        }

        Button* pressedBtn = whichButtonPressed();
        if(pressedBtn != NULL) { // if any button is actually pressed
            switch (pressedBtn->pin) { // this is basically a way to write a bunch of elifs super efficiently
                case BUTTON1:
                    localValue--;
                    if (localValue < 0) localValue = maxValue - 1;
                    break;
                
                case BUTTON2:
                    localValue++;
                    if (localValue >= maxValue) localValue = 0;
                    break;

                case BUTTON5:
                    unitSet = true;
                    break;
            }
            delay(100); //small delay
        }
    }

    result.updatedValue = localValue;
    result.unitFinished = true;
    return result;
}

void setClockTime() {
    resetScreen();
    tft.setTextSize(3);
    tft.print("Welcome User!");
    delay(500);
    for(int i = 0; i < timeUnitsListSize; i++) {
        bool currentTimeUnitSet = false;

        while(!currentTimeUnitSet) {
            switch(i){
                case 0: {
                    SetupResult report = chooseSetUnitOfTime(hours, "Hour", HOURS);
                    hours = report.updatedValue;
                    currentTimeUnitSet = report.unitFinished;
                    break;
                }
                case 1: {
                    SetupResult report = chooseSetUnitOfTime(minutes, "Minutes", MINUTES);
                    minutes = report.updatedValue;
                    currentTimeUnitSet = report.unitFinished;
                    break;
                }
                case 2: {
                    SetupResult report = chooseSetUnitOfTime(seconds, "Seconds", SECONDS);
                    seconds = report.updatedValue;
                    currentTimeUnitSet = report.unitFinished;
                    break;
                }
            }
        }
        resetScreen();
        delay(10);
    }
    timeSet = true;

    if(timeSet) {
        setupComplete = true;
        resetScreen();
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, LOW);
    
    for (int i = 0; i < NUM_BUTTONS; i++) {
        pinMode(clockButtons[i].pin, INPUT_PULLUP);
    }

    tft.init(76, 284); //display size
    tft.setOffsets(82, 18);
    tft.invertDisplay(false);
    tft.setRotation(1);

    tft.fillScreen(ST77XX_BLACK);

    setClockTime();

    clockTicker.attach(1.0, clockTick); //start the clock ticking
}

void loop() {
    if (!setupComplete) { //if setupComplete returns as false, don't run the rest of code in the loop function
        return; 
    }

    displayClock();

    bool currentModeBtnState = digitalRead(BUTTON1);
    if (currentModeBtnState == LOW && pastBM1 == HIGH) {
        changeClockMode();
    }
    pastBM1 = currentModeBtnState;
}