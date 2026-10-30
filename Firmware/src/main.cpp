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

#define ST77XX_GRAY 0x8410 //apparently GRAY isn't in the normal ST77XX so I had to define it here.

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

int alarmHour1 = 0;
int alarmMinute1 = 0;
int alarmSecond1 = 0;
bool alarmOneActive = false;

int alarmHour2 = 0;
int alarmMinute2 = 0;
int alarmSecond2 = 0;
bool alarmTwoActive = false;

int countdownAlarmHour = 0;
int countdownAlarmMinute = 0;
int countdownAlarmSecond = 0;
bool countdownAlarmActive = false;

bool triggerBuzzer = false;
bool turnOffTriggerBuzzer = false;

bool processGoing = false;

const char* timeUnits[] = {"Hours", "Minutes", "Seconds"};
int timeUnitsListSize = sizeof(timeUnits) / sizeof(timeUnits[0]);
enum Mode { HOURS, MINUTES, SECONDS };
Mode clockMode = HOURS;

struct SetupResult {
  int updatedValue;
  bool unitFinished;
};

struct returnValueAndStatus {
  int outputValue;
  int onOffEdit;
  bool processCompleted;
};


Ticker clockTicker;
Ticker cd_ClockTicker;
volatile int totalSeconds = 0;

void clockTick() {
  seconds = seconds + 1; //add a second to clock
  if (seconds >= 60) {
    seconds = 0;
    minutes = minutes + 1;
    if (minutes >= 60) {
      minutes = 0;
      hours = hours + 1;
      if (hours >= 24) {
        hours = 0;
      }
    }
  }
}

void cd_ClockTick() {
    totalSeconds = totalSeconds - 1;
}

struct Button {
    uint8_t pin;
    bool lastState;
    const char* name;
    unsigned long lastPressed; //creates timer for each button so it can have it's own debounce tracker/timer
};

Button clockButtons[] = {
    {BUTTON1, HIGH, "Button 1", 0},
    {BUTTON2, HIGH, "Button 2", 0},
    {BUTTON3, HIGH, "Button 3", 0},
    {BUTTON4, HIGH, "Button 4", 0},
    {BUTTON5, HIGH, "Button 5", 0}
};

const int NUM_BUTTONS = sizeof(clockButtons) / sizeof(clockButtons[0]);

bool buttonPressed(Button& button) {
    bool currentState = digitalRead(button.pin);
    bool triggered = false;
    unsigned long now = millis();

    if (currentState == LOW && button.lastState == HIGH) {
        if(now - button.lastPressed >= 50) { //if it's been at least 50 milliseconds since the last button press
            triggered = true;           
            button.lastPressed = now;
        }

    }  else if (currentState == LOW && button.lastState == LOW) {
        if(now - button.lastPressed >= 200) { //if it's been at least 200 milliseconds since the last button press
            button.lastPressed = now;
            triggered = true;
        }
    }

    button.lastState = currentState;
    return triggered;
}

bool anyButtonPressed() {
  for(int i = 0; i < NUM_BUTTONS; i++) {
    if(buttonPressed(clockButtons[i])) {
      return true;
    }
  }
  return false;
}

Button* whichButtonPressed() {
  for(int i = 0; i < NUM_BUTTONS; i++) {
    if(buttonPressed(clockButtons[i])) {
      return &clockButtons[i];
    }
  } 
  return NULL;
}

void resetScreen() {
    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(10,10);
}

void completeResetScreen() {
    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(0,0);    
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

void justPrintTheClock() {
    resetScreen();
    if (displayMode == 0) {
        resetScreen();
        printHourMinute();
    } else if (displayMode == 1) {
        resetScreen();
        printHourMinuteSecond();
    }    
}

SetupResult chooseSetUnitOfTime(int valueOfUnitOfMeasurement, const char* labelUnitOfMeasurement, Mode targetedMode) {
    SetupResult result;

    completeResetScreen();
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.printf("Set %s.", labelUnitOfMeasurement);
    tft.println(); //move down to next line below the first lines of text
    tft.println(); //move down another line to create a buffer between next text printed, and the text that's already printed
    int cursorX = tft.getCursorX();
    int cursorY = tft.getCursorY();
    tft.setTextSize(3);
    tft.printf("%s:", labelUnitOfMeasurement);
    cursorX = tft.getCursorX();
    cursorY = tft.getCursorY();

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
            tft.fillRect(cursorX, cursorY, 76, 284-cursorY, ST77XX_BLACK); //create rectangle that covers the bottom text, basically erasing it
            tft.setCursor(cursorX, cursorY);
            tft.setTextSize(3);
            tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
            tft.printf("%02d", localValue);
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

int alarmOnOffEdit() {
    processGoing = true;
    completeResetScreen();
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK); 
    tft.setTextSize(1);
    tft.print("Select Alarm Configuration Option");
    tft.println();
    tft.setTextSize(2);
    tft.print("Current:");
    tft.println();
    int cursorX = tft.getCursorX();
    int cursorY = tft.getCursorY();
    tft.setTextSize(3);
    tft.print("On");


    int onOffEditLast = 0;
    int onOffEdit = 0;
    bool onOffEditChosen = false;

    while(!onOffEditChosen) {
        Button* pressedBtn = whichButtonPressed();
        if(pressedBtn != NULL) { // if any button is actually pressed
            switch (pressedBtn->pin) { // this is basically a way to write a bunch of elifs super efficiently
                case BUTTON1:
                    onOffEdit--;
                    if (onOffEdit < 0) onOffEdit = 3;
                    break;
                
                case BUTTON2:
                    onOffEdit++;
                    if (onOffEdit >= 3) onOffEdit = 0;
                    break;
                
                case BUTTON5:
                    onOffEditChosen = true;
                    break;
            }

            if(onOffEdit != onOffEditLast) {
                switch(onOffEdit) {
                    case 0:
                        tft.fillRect(cursorX, cursorY, 76, 284-cursorY, ST77XX_BLACK); //create rectangle that covers the bottom text, basically erasing it
                        tft.setCursor(cursorX, cursorY);
                        tft.setTextSize(3);
                        tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
                        tft.print("On");
                        break;

                    case 1:
                        tft.fillRect(cursorX, cursorY, 76, 284-cursorY, ST77XX_BLACK); //create rectangle that covers the bottom text, basically erasing it
                        tft.setCursor(cursorX, cursorY);
                        tft.setTextSize(3);
                        tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
                        tft.print("Off");
                        break;

                    case 2:
                        tft.fillRect(cursorX, cursorY, 76, 284-cursorY, ST77XX_BLACK); //create rectangle that covers the bottom text, basically erasing it
                        tft.setCursor(cursorX, cursorY);
                        tft.setTextSize(3);
                        tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
                        tft.print("Edit");
                        break;
                }
            }

            onOffEditLast = onOffEdit;

            delay(100); //small delay
        }
    }

    return onOffEdit;
}

returnValueAndStatus changeAlarm() {
    returnValueAndStatus result;

    bool alarmChosen = false;
    int lastAlarmChosen = -1;
    int currentAlarmChosen = 1;
    int totalNumberOfAlarms = 2;
    int cursorX = 0;
    int cursorY = 0;

    processGoing = true;
    completeResetScreen();
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK); 
    tft.setTextSize(1);
    tft.print("Select Alarm:");
    tft.println();
    tft.setTextSize(3);
    tft.print("Alarm:");
    cursorX = tft.getCursorX();
    cursorY = tft.getCursorY();

    while(!alarmChosen) {
        if(currentAlarmChosen != lastAlarmChosen) {
            tft.fillRect(cursorX, cursorY, 76, 284-cursorY, ST77XX_BLACK); //create rectangle that covers the bottom text, basically erasing it
            tft.setCursor(cursorX, cursorY);
            tft.setTextSize(3);
            tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
            tft.printf("%02d", currentAlarmChosen);
            lastAlarmChosen = currentAlarmChosen;
        }

        Button* pressedBtn = whichButtonPressed();
        if(pressedBtn != NULL) { // if any button is actually pressed
            switch (pressedBtn->pin) { // this is basically a way to write a bunch of elifs super efficiently
                case BUTTON1:
                    currentAlarmChosen--;
                    if (currentAlarmChosen < 1) currentAlarmChosen = totalNumberOfAlarms;
                    break;
                
                case BUTTON2:
                    currentAlarmChosen++;
                    if (currentAlarmChosen > totalNumberOfAlarms) currentAlarmChosen = 1;
                    break;

                case BUTTON5:
                    alarmChosen = true;
                    break;
            }
            delay(100); //small delay
        }
    }
    resetScreen();

    int onOffEdit = alarmOnOffEdit();

    result.outputValue = currentAlarmChosen;
    result.onOffEdit = onOffEdit;
    result.processCompleted = true;

    return result;
}

void setAlarmTime(int alarmNumber) {
    resetScreen();

    int localAlarmHour = 0;
    int localAlarmMinute = 0;
    int localAlarmSecond = 0;

    for(int i = 0; i < 2; i++) {
        bool currentTimeUnitSet = false;

        while(!currentTimeUnitSet) {
            switch(i){
                case 0: {
                    SetupResult report = chooseSetUnitOfTime(localAlarmHour, "Hour", HOURS);
                    localAlarmHour = report.updatedValue;
                    currentTimeUnitSet = report.unitFinished;
                    break;
                }
                case 1: {
                    SetupResult report = chooseSetUnitOfTime(localAlarmMinute, "Minutes", MINUTES);
                    localAlarmMinute = report.updatedValue;
                    currentTimeUnitSet = report.unitFinished;
                    break;
                }
                case 2: {
                    localAlarmSecond = 0;
                    currentTimeUnitSet = true;
                    break;
                }
            }
        }
        if(alarmNumber == 1) {
            alarmHour1 = localAlarmHour;
            alarmMinute1 = localAlarmMinute;
            alarmSecond1 = localAlarmSecond;
        } else if(alarmNumber == 2) {
            alarmHour2 = localAlarmHour;
            alarmMinute2 = localAlarmMinute;
            alarmSecond2 = localAlarmSecond;
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

int countdownAlarm() {
    resetScreen();

    int localAlarmHour = 0;
    int localAlarmMinute = 0;
    int localAlarmSecond = 0;

    for(int i = 0; i < timeUnitsListSize; i++) {
        bool currentTimeUnitSet = false;

        while(!currentTimeUnitSet) {
            switch(i){
                case 0: {
                    SetupResult report = chooseSetUnitOfTime(localAlarmHour, "Hour", HOURS);
                    localAlarmHour = report.updatedValue;
                    currentTimeUnitSet = report.unitFinished;
                    break;
                }
                case 1: {
                    SetupResult report = chooseSetUnitOfTime(localAlarmMinute, "Minutes", MINUTES);
                    localAlarmMinute = report.updatedValue;
                    currentTimeUnitSet = report.unitFinished;
                    break;
                }
                case 2: {
                    SetupResult report = chooseSetUnitOfTime(localAlarmSecond, "Seconds", SECONDS);
                    localAlarmSecond = report.updatedValue;
                    currentTimeUnitSet = report.unitFinished;
                    break;
                }
            }
        }
    }

    countdownAlarmHour = localAlarmHour;
    countdownAlarmMinute = localAlarmMinute;
    countdownAlarmSecond = localAlarmSecond;

    volatile int totalSeconds = ((localAlarmHour * 3600) + (localAlarmMinute * 60) + localAlarmSecond);

    delay(10);

    bool arm_yes_no = false;
    bool decision_made = false;
    bool current_decision = true;
    bool last_decision = false;

    tft.fillScreen(ST77XX_RED);
    tft.setCursor(0,0);
    tft.setTextColor(ST77XX_WHITE, ST77XX_RED); 
    tft.setTextSize(1);
    tft.print("ARM?");
    tft.println();

    int answer_cursorX = tft.getCursorX();
    int answer_cursorY = tft.getCursorY();
    
    Button* pressedBtn = whichButtonPressed();
    while(!decision_made) {
        if(current_decision != last_decision) {
            if(current_decision) {
                tft.setCursor(answer_cursorX, answer_cursorY);
                tft.setTextSize(3);
                tft.print("YES");
            } else {
                tft.setCursor(answer_cursorX, answer_cursorY);
                tft.setTextSize(3);
                tft.print("NO");
            }
        }


        if(pressedBtn != NULL) {
            switch (pressedBtn->pin) {
                case BUTTON1: {
                    current_decision = !current_decision;
                    break;
                }

                case BUTTON5: {
                    arm_yes_no = current_decision;
                    completeResetScreen();
                    decision_made = true;
                    break;
                }
            }
        }
    }

    if(!arm_yes_no) {
        justPrintTheClock();
        processGoing = false;
        return 0;
    }

    for(int i = 0; i < 3; i++) {
        tft.fillScreen(ST77XX_RED);
        tft.setCursor(0,0);
        tft.setTextColor(ST77XX_WHITE, ST77XX_RED); 
        tft.setTextSize(4);
        tft.print("ARMED");
        delay(300);
        completeResetScreen();
        delay(300);
    }

    cd_ClockTicker.attach(1.0, cd_ClockTick);

    int cd_lastTotalSecond = 0;
    int cd_seconds = 0;
    int cd_minutes = 0;
    int cd_hours = 0;
    int cd_lastseconds = -1;
    int cd_lastminutes = -1;
    int cd_lasthours = -1;

    cd_hours = totalSeconds / 3600; //NOTE: CHECK IF THIS CALCULATES TIME PROPERLY LATER PLS
    cd_minutes = ((totalSeconds % 3600) / 60);
    cd_seconds = totalSeconds % 60;

    tft.setCursor(10, 10);
    tft.setTextColor(ST77XX_WHITE, ST77XX_RED);
    tft.setTextSize(4);

    int hours_cursorX = tft.getCursorX();
    int hours_cursorY = tft.getCursorY();
    tft.printf("%02d:", cd_hours);

    int minutes_cursorX = tft.getCursorX();
    int minutes_cursorY = tft.getCursorY();
    tft.printf("%02d:", cd_minutes);

    int seconds_cursorX = tft.getCursorX();
    int seconds_cursorY = tft.getCursorY();

    int code_length = 5; //length of code
    int code[code_length];
    for(int i = 0; i<code_length; i++) {
        code[i] = random(1, 6);
    }

    tft.printf("%02d", cd_seconds);

    int focused_digit = 0;

    tft.setTextColor(ST77XX_WHITE, ST77XX_RED);
    tft.setTextSize(3);
    tft.print("Code:[");
    int digit_cursorY = tft.getCursorY();
    int digit1_cursorX = tft.getCursorX();
    tft.setTextColor(ST77XX_GREEN, ST77XX_RED);
    tft.print("  ");
    int digit2_cursorX = tft.getCursorX();
    tft.setTextColor(ST77XX_WHITE, ST77XX_RED);
    tft.setTextSize(4);
    tft.printf("%1d", code[focused_digit]);
    int digit3_cursorX = tft.getCursorX();
    tft.setTextColor(ST77XX_GRAY, ST77XX_RED);
    tft.setTextSize(3);
    tft.printf("%1d", code[(focused_digit+1)]);

    bool new_input = false;
    bool defused = false;
    int current_input_digit = 0;

    while((totalSeconds > 0) && (!defused)) {
        if (cd_lastTotalSecond != totalSeconds){
            tft.setCursor(10, 10);
            tft.setTextColor(ST77XX_RED, ST77XX_BLACK);
            tft.setTextSize(4);

            cd_hours = totalSeconds / 3600; //NOTE: CHECK IF THIS CALCULATES TIME PROPERLY LATER PLS
            cd_minutes = ((totalSeconds % 3600) / 60);
            cd_seconds = totalSeconds % 60;

            if(cd_lasthours != cd_hours){
                tft.setCursor(hours_cursorX, hours_cursorY);
                tft.printf("%02d", cd_hours);
            }

            if(cd_lastminutes != cd_minutes){
                tft.setCursor(minutes_cursorX, minutes_cursorY);
                tft.printf("%02d", cd_minutes);
            }

            if(cd_lastseconds != cd_seconds){
                tft.setCursor(seconds_cursorX, seconds_cursorY);
                tft.printf("%02d", cd_seconds);
            }

            cd_lasthours = cd_hours;
            cd_lastminutes = cd_minutes;
            cd_lastseconds = cd_seconds;
            cd_lastTotalSecond = totalSeconds;
        }
        if(pressedBtn != NULL) {
            switch (pressedBtn->pin) {
                case BUTTON1: {
                    current_input_digit = 1;
                    new_input = true;
                    break;
                }
                case BUTTON2: {
                    current_input_digit = 2;
                    new_input = true;
                    break;
                }
                case BUTTON3: {
                    current_input_digit = 3;
                    new_input = true;
                    break;
                }
                case BUTTON4: {
                    current_input_digit = 4;
                    new_input = true;
                    break;
                }
                case BUTTON5: {
                    current_input_digit = 5;
                    new_input = true;
                    break;
                }
            }
        }
        if(new_input){
            if ((current_input_digit == code[focused_digit]) && (focused_digit < (code_length-1))){
                focused_digit++;
                tft.setTextSize(3);
                tft.setCursor(digit1_cursorX, digit_cursorY);
                tft.setTextColor(ST77XX_GREEN, ST77XX_RED);
                tft.printf("%1d", code[(focused_digit-1)]);

                tft.setCursor(digit2_cursorX, digit_cursorY);
                tft.setTextColor(ST77XX_WHITE, ST77XX_RED);
                tft.setTextSize(4);
                tft.printf("%1d", code[focused_digit]);

                tft.setCursor(digit3_cursorX, digit_cursorY);
                tft.setTextColor(ST77XX_GRAY, ST77XX_RED);
                tft.setTextSize(3);
                tft.printf("%1d", code[(focused_digit+1)]);

            } else if ((current_input_digit = code[focused_digit]) && (focused_digit = (code_length-1))) {
                defused = true;
            }
            new_input = false;
        }
    }

    cd_ClockTicker.detach();
    
    if(defused) {
        for(int i = 0; i < 3; i++) {
            tft.fillScreen(ST77XX_GREEN);
            tft.setCursor(0,0);
            tft.setTextColor(ST77XX_WHITE, ST77XX_GREEN); 
            tft.setTextSize(4);
            tft.print("DISARMED");
            delay(300);
            completeResetScreen();
            delay(300);
        }
        justPrintTheClock();
        processGoing = false;
        return 1;
    } else if ((!defused) && (!(totalSeconds > 0))) {
        triggerBuzzer = true;
        for(int i = 0; i < 3; i++) {
            tft.fillScreen(ST77XX_RED);
            tft.setCursor(0,0);
            tft.setTextColor(ST77XX_WHITE, ST77XX_RED); 
            tft.setTextSize(4);
            tft.print("DEFUSAL FAILED");
            delay(300);
            completeResetScreen();
            delay(300);
        }
        delay(500);
        return 2;
    }
    return 0;
}

void turnAlarmOnOrOff(int alarmNumber, bool turnOnOrOff) {
    if(alarmNumber == 1) {
        alarmOneActive = turnOnOrOff;
    } else if(alarmNumber == 2) {
        alarmTwoActive = turnOnOrOff;
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, LOW); //display (for some weird reason) is active low, so setting it to low makes sure backlight is on
    pinMode(BUZZER, OUTPUT);
    digitalWrite(BUZZER, LOW);
    
    for(int i = 0; i < NUM_BUTTONS; i++) {
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
    if(!setupComplete) { //if still in the middle of setup (like setting the time), skip over rest of code in loop
        return;
    }

    if(processGoing) { //if there's an ongoing process, skip over rest of code in loop
        return;
    }

    displayClock();

    Button* pressedBtn = whichButtonPressed();

    if(pressedBtn != NULL) { // if any button is actually pressed
        switch (pressedBtn->pin) { // this is basically a way to write a bunch of elifs super efficiently
            case BUTTON1: {
                changeClockMode();
                break;
            }
            
            case BUTTON2: {
                processGoing = true;
                returnValueAndStatus alarmChosenNumber = changeAlarm();
                
                if(alarmChosenNumber.processCompleted == true) {
                    if(alarmChosenNumber.onOffEdit == 0) {
                        turnAlarmOnOrOff(alarmChosenNumber.outputValue, true);
                        resetScreen();
                        tft.setTextSize(2);
                        tft.print("Alarm turned on");
                        delay(500);
                    } else if (alarmChosenNumber.onOffEdit == 1) {
                        turnAlarmOnOrOff(alarmChosenNumber.outputValue, false);
                        resetScreen();
                        tft.setTextSize(2);
                        tft.print("Alarm turned off");
                        delay(500); 
                    } else if (alarmChosenNumber.onOffEdit == 2) {
                        setAlarmTime(alarmChosenNumber.outputValue);
                    }
                }

                processGoing = false;
                justPrintTheClock();
                break;
            }

            case BUTTON3: {
                processGoing = true;
                int beh_beh = countdownAlarm();
                Serial.print(beh_beh);
                break;
            }
            
            case BUTTON4: {
                break;
            }

            case BUTTON5: {
                turnOffTriggerBuzzer = true;
                break;
            }
        }
        delay(100); //small delay
    }

    if(alarmOneActive) {
        if(hours == alarmHour1 && minutes == alarmMinute1 && seconds == alarmSecond1) {
            triggerBuzzer = true;
        }
    } 
    if(alarmTwoActive) {
        if(hours == alarmHour2 && minutes == alarmMinute2 && seconds == alarmSecond2) {
            triggerBuzzer = true;
        }
    }

    if(triggerBuzzer) {
        digitalWrite(BUZZER, HIGH); //disabled for now so buzzer doesn't bug me while I'm testing the clock
        triggerBuzzer = false;
    }

    if(turnOffTriggerBuzzer) {
        digitalWrite(BUZZER, LOW);
        turnOffTriggerBuzzer = false;
    }
}