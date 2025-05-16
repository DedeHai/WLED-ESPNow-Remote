/**********************************************************************
  ESPNow remote for WLED
  Copyright (c) 2024  Damian Schneider (@DedeHai)
  Licensed under the EUPL v. 1.2 or later
  https://github.com/DedeHai/WLED-ESPNow-Remote
***********************************************************************/


//#define DISABLE_DEEPSLEEP // uncomment to disable going to deepsleep: any GPIO can be used for a button at the expense of ~8x shorter battery life

//hardware configuration  note: using both ROTARY_ENCODER and BUTTON_MATRIX may work but is untested
//#define ROTARY_ENCODER // uncomment to use rotary encoder (can have additional buttons)
//#define BUTTON_MATRIX  // uncomment to use a button matrix as input
#define SENSOR_TRIGGER // uncomment to use a digital sensor trigger with an active high output like a PIR Sensor  ->unfinished

/////////////////////////////
// ROTARY ENCODER SETTINGS //
/////////////////////////////
#if defined(ROTARY_ENCODER)
//#define ENABLE_DEEPSLEEP_PULL  // uncomment to use internal pullup on CLK pin during deep sleep (uses an extra 70-100uA, highly recommended to use 1M - 2M external pullup on CLK line, no pullup on DT line and use GPIO2 for button. Internal can be used when no external resistors are available)
#define NUM_BUTTONS 1            // rotary encoder only hase one button but it is possible to add more
#define NUM_ROTARYLEVELS 3       // number of rotary levels: double click on rotary button goes to next level, sending higher numbered button codes, can be set as large as 127. Set to 1 if not using levels. DO NOT SET TO 0
#define ACTIVE_TIME 2000         // time in milliseconds to stay active (i.e. in light sleep handling inputs) before going back to deep sleep (in deep sleep, first one or two rotary positions are missed until awake, even when using wake-up stub, should be detecting button press without issues)
#define KEEPROTARYLEVEL_TIME 40  // time in seconds until rotaryLevel is reset to 0 after ACTIVE_TIME has passed
// rotary encoder pins (can only use RTC pins, GPIO0-GPIO5), if direction is inverted, flip CLK and DT. Highly recommended to use GPIO2 for button pin as it has an on-board pullup, do not use GPIO2 for CLK or DT (increased deep sleep current)
#define CLK_PIN 1
#define DT_PIN 0
#define ROTBUTTON_PIN 2
RTC_DATA_ATTR uint8_t buttonPins[NUM_BUTTONS] = { ROTBUTTON_PIN };  // can add more buttons if needed, ROTBUTTON_PIN must be at index 0

////////////////////////////
// BUTTON MATRIX SETTINGS //
////////////////////////////
#elif defined(BUTTON_MATRIX)
#define MATRIX_COLS 4    // number of columns, connect colums to GPIO 0-5, they are used as interrupt inputs
#define MATRIX_ROWS 4    // number of rows, connect to any GPIO except GPIO9 (boot pin), if at all possible avoid pins with pullups to save power (i.e. GPIO2, GPIO8 on the C3 supermini)
#define NUM_BUTTONS (MATRIX_COLS * MATRIX_ROWS)    // change assigned function numbers in initButtons()
RTC_DATA_ATTR uint8_t columnPins[MATRIX_COLS] = { 0, 1, 2, 3 };   // column input pins, must be RTC capable pins (GPIO 0-5). These are usually marked C1, C2,... on the matrix
RTC_DATA_ATTR uint8_t rowPins[MATRIX_ROWS] = { 5, 6, 7, 10 };     // row output scanning pins, do not use GPIO9 (or it may not boot) avoid GPIO8 (LED pin). These are usually marked R1, R2,... on the matrix
#define ACTIVE_TIME 4000  // time in milliseconds to stay active (i.e. in light sleep handling inputs) before going back to deep sleep note: making this short, like 1s, can lead to missing buttons (probably while entering deep sleep)

///////////////////////////
// SENSOR INPUT SETTINGS //
///////////////////////////
#elif defined(SENSOR_TRIGGER)
//TODO: for each sensor, need to define low active or high active
#define NUM_SENSORS 1    // number of sensor inputs to use, up to 5 possible: GPIO0-GPIO5 but avoid GPIO2 (increased power due to pullup)
#define NUM_BUTTONS NUM_SENSORS // sensor input uses button logic to track the pin state
RTC_DATA_ATTR uint8_t sensorPins[NUM_SENSORS] = { 0 };   // column input pins, must be RTC capable pins (GPIO 0-5). These are usually marked C1, C2,... on the matrix
#define ACTIVE_TIME 4000  // time in milliseconds to stay active (i.e. in light sleep handling inputs) before going back to deep sleep note: making this short, like 1s, can lead to missing buttons (probably while entering deep sleep)

#ifdef SENSOR_TRIGGER
  for (int i = 0; i < NUM_SENSORS; i++) {
    pinMode(sensorPins[i], INPUT);  // sensor with active output needs no pullup/pulldown
  }
#endif

//////////////////////////////
// STANDARD BUTTON SETTINGS //
//////////////////////////////
#else                      // normal buttons directly connected to GPIOs (external pullups are not required)
#define NUM_BUTTONS 6      // define the number of buttons and the used pins (only GPIO0-5 can be used to wake from deepsleep, if using other GPIOs, they are only detectable during ACTIVE_TIME, avoid GPIO9 = boot pin)
RTC_DATA_ATTR uint8_t buttonPins[NUM_BUTTONS] = { 0, 1, 2, 3, 4, 5 };  // GPIOs to use for buttons, change assigned function numbers in initButtons()
#define ACTIVE_TIME 4000   // time in milliseconds to stay active (i.e. in light sleep handling inputs & sendouts) before going back to deep sleep note: making this short, like 1s, can lead to missing buttons (probably while entering deep sleep)
#endif