/**********************************************************************
  ESPNow remote for WLED
  Copyright (c) 2024  Damian Schneider (@DedeHai)
  Licensed under the EUPL v. 1.2 or later
  https://github.com/DedeHai/WLED-ESPNow-Remote
***********************************************************************/

// change the code below to assign function numbers to buttons, use WLED with debug output enabled or the ESPNow_receiver test sketch on a second ESP to check

#define DEFAULTS_START 1  // buttons starting at this index (inclusive) and up are assigned the default function (function numbers 16 + i*3, see code below)

// initialize button states and assign button functions
void initButtons(void) {
  for (int i = 0; i < NUM_BUTTONS; i++) {
#ifndef BUTTON_MATRIX
    pinMode(buttonPins[i], INPUT_PULLUP);
#endif
    buttons[i].longPress = false;
    buttons[i].lastchange = 0;
    // note: buttonPressed and buttonClicks are set in deepsleep wake-up stub
  }

#ifdef SENSOR_TRIGGER
  for (int i = 0; i < NUM_SENSORS; i++) {
    pinMode(sensorPins[i], INPUT);  // sensor with active output needs no pullup/pulldown
  }
#endif

#ifdef BUTTON_MATRIX
  for (int i = 0; i < MATRIX_COLS; i++) {
    pinMode(columnPins[i], INPUT_PULLUP);     // column pins are used for reading button state
    gpio_hold_en((gpio_num_t)columnPins[i]);  // hold state during sleep
  }
  for (int i = 0; i < MATRIX_ROWS; i++) {
    pinMode(rowPins[i], OUTPUT);  // row pins are used for scanning, start out low
    digitalWrite(rowPins[i], LOW);
    gpio_hold_en((gpio_num_t)rowPins[i]);  // hold state during sleep
  }
#endif

  //////////////////////////////
  // BUTTON FUNCTION SETTINGS //
  //////////////////////////////

  // assign button function number to be sent out: use remote.json file in WLED to assign a json command to each button function. without a remote.json file, WLED defaults to WiZmote defined functions and other function numbers are ignored
  // any number from 1-255 can be used, assigning 0 will skip sending a command
  // when using a (correctly connected) BUTTON_MATRIX the numbering increments left to right on each column, so for example with 4 columns, the first row is 0, 1, 2, 3, second row is 4, 5, 6, 7 etc.
  buttons[0].singleClickFunction = WIZMOTE_BUTTON_ON;
  buttons[0].doubleClickFunction = 0;  // not used  note: rotary encoder button uses this to switch levels, a function can still be assigned if required, it will be sent out on level change
  buttons[0].longPressFunction = WIZMOTE_BUTTON_OFF;

  // examples:
  //buttons[1].singleClickFunction = WIZMOTE_BUTTON_NIGHT;
  //buttons[1].doubleClickFunction = 0; // not used
  //buttons[1].longPressFunction = WIZMOTE_BUTTON_OFF;

  //buttons[2].singleClickFunction = WIZMOTE_BUTTON_BRIGHT_UP;
  //buttons[2].doubleClickFunction = WIZMOTE_BUTTON_BRIGHT_UP;
  //buttons[2].longPressFunction = WIZMOTE_BUTTON_BRIGHT_UP;

  //buttons[3].singleClickFunction = WIZMOTE_BUTTON_BRIGHT_DOWN;
  //buttons[3].doubleClickFunction = WIZMOTE_BUTTON_BRIGHT_DOWN;
  //buttons[3].longPressFunction = WIZMOTE_BUTTON_BRIGHT_DOWN;

  // assign default function numbers: WIZMOTE_BUTTON_ONE = 16 and up
  for (int i = DEFAULTS_START; i < NUM_BUTTONS; i++) {
    uint8_t base = WIZMOTE_BUTTON_ONE + i * 3;  // assign function number starting at WIZMOTE_BUTTON_ONE (19), increment for each button sub-function
    buttons[i].singleClickFunction = base;
    buttons[i].doubleClickFunction = base + 1;
    buttons[i].longPressFunction = base + 2;
  }
  handleButtons();  // update states
}

bool IRAM_ATTR anyButtonPressed(void) {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (buttons[i].buttonPressed) return true;
  }
  return false;
}


#ifdef BUTTON_MATRIX
// scans and updates button states of a button matrix, takes 100us if not buttons pressed, ~600us if buttons are pressed
void IRAM_ATTR scanButtonMatrix(void) {
  bool columnactive[MATRIX_COLS] = { false };
  bool doScan = false;

  // reset all button states to high (=not pressed)
  for (int i = 0; i < NUM_BUTTONS; i++) {
    currentButtonState[i] = HIGH;
  }

  // pre-scan columns to determine which columns to scan
  for (int i = 0; i < MATRIX_COLS; i++) {
    if (digitalRead(columnPins[i]) == LOW) {  // with all row pins set to low, we can check a full column at once, if it is low, at least one button in this column is pressed, so scan it
      columnactive[i] = true;
      doScan = true;
    }
  }

  if (doScan) {
    for (int j = 0; j < MATRIX_ROWS; j++) {
      gpio_hold_dis((gpio_num_t)rowPins[j]);  // disable hold so we can change the GPIO setting
      pinMode(rowPins[j], INPUT_PULLUP);      // set row pins to pullup
    }

    for (int i = 0; i < MATRIX_COLS; i++) {
      if (columnactive[i]) {                     // found active signal in this column, scan it
        for (int j = 0; j < MATRIX_ROWS; j++) {  // scan the rows
          digitalWrite(rowPins[j], LOW);
          pinMode(rowPins[j], OUTPUT);
          delayMicroseconds(10);  // wait for pin voltage to be stable
          currentButtonState[i + MATRIX_COLS * j] = digitalRead(columnPins[i]);
          pinMode(rowPins[j], INPUT_PULLUP);  // set back to pullup before doing next row
        }
      }
    }
  }

  // set row pins to idle state (output low)
  for (int j = 0; j < MATRIX_ROWS; j++) {
    pinMode(rowPins[j], OUTPUT);  // row pins are used for scanning, start out low
    digitalWrite(rowPins[j], LOW);
    gpio_hold_en((gpio_num_t)rowPins[j]);  // hold state during sleep
  }
}
#endif

void IRAM_ATTR readButtons() {
#ifdef BUTTON_MATRIX
  scanButtonMatrix();
#else  // normal buttons
  for (int i = 0; i < NUM_BUTTONS; i++) {
    currentButtonState[i] = digitalRead(buttonPins[i]);
  }
#endif
}
