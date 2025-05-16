/**********************************************************************
  ESPNow remote for WLED
  Copyright (c) 2024  Damian Schneider (@DedeHai)
  Licensed under the EUPL v. 1.2 or later
  https://github.com/DedeHai/WLED-ESPNow-Remote
***********************************************************************/


#include "esp_sleep.h"
#include "rom/gpio.h"  // Include ROM GPIO functions (required for wake-up stub)

#define BUTTONDEGLITCHTIME 1000  // time in microseconds to wait after a button GPIO triggered wake-up

// enter light sleep, wake up after duration in microseconds
void enterLightSleep(unsigned micros) {
  esp_sleep_enable_gpio_wakeup();
  esp_sleep_enable_timer_wakeup(micros);  // wake up time is in microseconds

#ifdef ROTARY_ENCODER
  gpio_sleep_set_pull_mode((gpio_num_t)CLK_PIN, GPIO_PULLUP_ONLY);
  gpio_sleep_set_pull_mode((gpio_num_t)DT_PIN, GPIO_PULLUP_ONLY);
  // Configure light sleep wake-up for the correct level: inverse of current state
  if (digitalRead(CLK_PIN))
    gpio_wakeup_enable((gpio_num_t)CLK_PIN, GPIO_INTR_LOW_LEVEL);  // pin is high, wake-up at low
  else
    gpio_wakeup_enable((gpio_num_t)CLK_PIN, GPIO_INTR_HIGH_LEVEL);
#endif

#ifdef BUTTON_MATRIX  // prepare matrix pins for sleep, hold mode is already set in pin setup, row pins are set to correct state in setup and after each scan
  for (int i = 0; i < MATRIX_COLS; i++) {
    gpio_sleep_set_pull_mode((gpio_num_t)columnPins[i], GPIO_PULLUP_ONLY);
    if (digitalRead(columnPins[i]))
      gpio_wakeup_enable((gpio_num_t)columnPins[i], GPIO_INTR_LOW_LEVEL);  // pin is high, wake-up at low
    else
      gpio_wakeup_enable((gpio_num_t)columnPins[i], GPIO_INTR_HIGH_LEVEL);
  }
#elifdef SENSOR_TRIGGER // active digital sensor (like PIR or sound trigger)
  for (int i = 0; i < NUM_SENSORS; i++) {
    gpio_sleep_set_pull_mode((gpio_num_t)sensorPins[i], GPIO_FLOATING);
    if (digitalRead(buttonPins[i]))
      gpio_wakeup_enable((gpio_num_t)sensorPins[i], GPIO_INTR_LOW_LEVEL);  // pin is high, wake-up at low
    else
      gpio_wakeup_enable((gpio_num_t)sensorPins[i], GPIO_INTR_HIGH_LEVEL);
  }
#else  // normal buttons
  for (int i = 0; i < NUM_BUTTONS; i++) {
    gpio_sleep_set_pull_mode((gpio_num_t)buttonPins[i], GPIO_PULLUP_ONLY);
    if (digitalRead(buttonPins[i]))
      gpio_wakeup_enable((gpio_num_t)buttonPins[i], GPIO_INTR_LOW_LEVEL);  // pin is high, wake-up at low
    else
      gpio_wakeup_enable((gpio_num_t)buttonPins[i], GPIO_INTR_HIGH_LEVEL);
  }
#endif
  // Enter light sleep
  esp_light_sleep_start();
}


// function sleeps but wakes and handles upon GPIO action, if fulltime is set, it will go back to sleep until the full time has elapsed (can be used for low power delay)
void sleepAndMonitor(unsigned micros, bool fulltime) {
  uint32_t sleepstart = esp_timer_get_time();
  enterLightSleep(micros);
  esp_sleep_wakeup_cause_t wakeupCause = esp_sleep_get_wakeup_cause();

  if (wakeupCause == ESP_SLEEP_WAKEUP_GPIO) {  // rotation or button press happened
#ifdef ROTARY_ENCODER
    handleRotaryEncoder(false);
#endif
    enterLightSleep(BUTTONDEGLITCHTIME);  // wait for button pin voltage to stabilize after an interrupt (deglitch)
  }

  handleButtons();  // poll buttons, detect long-press and timeout for single-click

  if (fulltime) {
    uint32_t timepassed = esp_timer_get_time() - sleepstart;
    if (timepassed < micros) {
      uint32_t timeleft = micros - timepassed;
      if (timeleft > 500)                     //skip short sleeps (might cause send errors)
        sleepAndMonitor(timeleft, fulltime);  // start from the top
    }
  }
}

// Function to enter deep sleep
void enterDeepSleep(unsigned micros) {
  if (micros > 0)
    esp_sleep_enable_timer_wakeup(micros);  // wake up time is in microseconds
  else
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);  // Disable timer-based wake-up (used for light sleep)

  //gpio_force_unhold_all();  // unhold all pins (just in case) -> does not seem to be working right...
  LED_OFF;  // make sure LED is off before entering deep sleep (LED macros use pin hold for sleep debugging)

#ifdef ROTARY_ENCODER
  // set wake-up state opposite to current state -> this will enable pullup or pulldown, cant use it or it drains the battery (its a feature not a bug :p )
  if (digitalRead(CLK_PIN)) {  //pin high
    esp_deep_sleep_enable_gpio_wakeup(1 << CLK_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
  } else
    esp_deep_sleep_enable_gpio_wakeup(1 << CLK_PIN, ESP_GPIO_WAKEUP_GPIO_HIGH);

  gpio_sleep_set_direction((gpio_num_t)CLK_PIN, GPIO_MODE_INPUT);
  gpio_sleep_set_direction((gpio_num_t)DT_PIN, GPIO_MODE_INPUT);
  gpio_sleep_set_pull_mode((gpio_num_t)CLK_PIN, GPIO_FLOATING);  // note: this is probably redundant if using hold mode
  gpio_sleep_set_pull_mode((gpio_num_t)DT_PIN, GPIO_FLOATING);
  pinMode(CLK_PIN, INPUT);
  pinMode(DT_PIN, INPUT);

#ifdef ENABLE_DEEPSLEEP_PULL  // enable CLK pullup in deepsleep
  pinMode(CLK_PIN, INPUT_PULLUP);
#endif
  gpio_hold_en((gpio_num_t)CLK_PIN);  // hold: fixes auto pull, see comment below
#endif                                // end ROTARY_ENCODER

// note: deep sleep is only triggered when no buttons are pressed so all inputs can simply be set to trigger low
#ifdef BUTTON_MATRIX
  for (int i = 0; i < MATRIX_COLS; i++) {
    pinMode(columnPins[i], INPUT_PULLUP);
    gpio_sleep_set_direction((gpio_num_t)columnPins[i], GPIO_MODE_INPUT);
    gpio_sleep_set_pull_mode((gpio_num_t)columnPins[i], GPIO_PULLUP_ONLY);  // note: this is probably redundant if using hold mode
    esp_deep_sleep_enable_gpio_wakeup(1 << columnPins[i], ESP_GPIO_WAKEUP_GPIO_LOW);
    gpio_hold_en((gpio_num_t)columnPins[i]);  // hold: fixes auto pull, see comment below (not really required for button pins)
  }
#else
  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
    gpio_sleep_set_direction((gpio_num_t)buttonPins[i], GPIO_MODE_INPUT);
    gpio_sleep_set_pull_mode((gpio_num_t)buttonPins[i], GPIO_PULLUP_ONLY);  // note: this is probably redundant if using hold mode
    esp_deep_sleep_enable_gpio_wakeup(1 << buttonPins[i], ESP_GPIO_WAKEUP_GPIO_LOW);
    gpio_hold_en((gpio_num_t)buttonPins[i]);  // hold: fixes auto pull (not really required for button pins)
  }
#endif
  gpio_deep_sleep_hold_en();  // hold: fix for a "feature" (see see https://www.esp32.com/viewtopic.php?t=23715) if hold is not set, deep_sleep_start() will automatically enable pullup/pulldown according to set wakeup level, this prevents that
  // Enter deep sleep
  esp_deep_sleep_start();  // note: this will set pullup/pullodown opposite to set gpio wake-up level (a bug in espressif IDF that can not be disabled when using arduino IDE)
}

// deep-sleep wake-up stub function (executed in RTC ram, so cannot use any flash or normal RAM residing functions ->register access directly)
void RTC_IRAM_ATTR esp_wake_deep_sleep(void) {
  esp_default_wake_deep_sleep();
  //gpio_force_unhold_all(); // release all pins from hold -> seems to not work, using individual pin unhold works

  // Configure LED_PIN as output low (for debug or delay test)
  //gpio_pad_unhold(LED_PIN);       // release from hold
  //gpio_output_set(0, 1 << LED_PIN, 1 << LED_PIN, 0); // enable and set LED_PIN LOW (trigger delay test)
  //gpio_output_set(0, 0, 0, 1 << LED_PIN); // disable LED_PIN output

#ifdef ROTARY_ENCODER
  // Configure CLK_PIN as input with pull-up -> it is too late, the pulse is already gone at this point (time to execution of stub function is ~12ms)
  gpio_pad_unhold(CLK_PIN);                             // release from hold
  gpio_pad_select_gpio(CLK_PIN);                        // set as GPIO function
  gpio_pad_pullup(CLK_PIN);                             // enable pull-up resistor
  gpio_pad_input_enable(CLK_PIN);                       // eable input mode
  lastClkState = (gpio_input_get() >> CLK_PIN) & BIT0;  //  GPIO_INPUT_GET(CLK_PIN);

  // Configure DT_PIN as input with pull-up
  gpio_pad_unhold(DT_PIN);        // release from hold
  gpio_pad_select_gpio(DT_PIN);   // set as GPIO function
  gpio_pad_pullup(DT_PIN);        // enable pull-up resistor
  gpio_pad_input_enable(DT_PIN);  // eable input mode
#endif

  // note: the stub is executed ~13ms after the wake interrupt, time to setup() is 47ms after wake interrupt
  // a fast key press can be low for only 20ms (less is hard to do) so 50ms in the setup is too late to read fast key inputs but we can read keys here in the stub

#ifdef BUTTON_MATRIX
  // scan all buttons (uses same prescan technique as normal readout)
  bool columnactive[MATRIX_COLS] = { false };

  // pre-scan columns to determine which columns to scan
  for (int i = 0; i < MATRIX_COLS; i++) {
    gpio_pad_unhold(columnPins[i]);                                 // release from hold
    gpio_pad_select_gpio(columnPins[i]);                            // Select GPIO function
    gpio_pad_pullup(columnPins[i]);                                 // Enable pull-up resistor
    gpio_pad_input_enable(columnPins[i]);                           // Enable input mode
    bool buttonState = (gpio_input_get() >> columnPins[i]) & BIT0;  // read button state
    if ((gpio_input_get() >> columnPins[i]) & BIT0 == LOW) {        // with all row pins set to low, we can check a full column at once, if it is low, at least one button in this column is pressed, so scan it
      columnactive[i] = true;
    }
  }

  // set row pins to input pullup in preparation for scanning
  for (int j = 0; j < MATRIX_ROWS; j++) {
    gpio_pad_unhold(rowPins[j]);        // release from hold
    gpio_pad_select_gpio(rowPins[j]);   // Select GPIO function
    gpio_pad_pullup(rowPins[j]);        // Enable pull-up resistor
    gpio_pad_input_enable(rowPins[j]);  // Enable input mode
  }

  for (int i = 0; i < MATRIX_COLS; i++) {
    if (columnactive[i]) {                                              // found active signal in this column, scan it
      for (int j = 0; j < MATRIX_ROWS; j++) {                           // scan the rows
        gpio_output_set(0, 1 << rowPins[j], 1 << rowPins[j], 0);        // enable output and set pin low
        for (int d = 0; d < 400; d++) __asm__ __volatile__("nop");      // short delay without timer available (measured: 5us)
        bool buttonState = (gpio_input_get() >> columnPins[i]) & BIT0;  // read column GPIO state
        if (buttonState == LOW) {
          buttons[i + MATRIX_COLS * j].buttonClicks++;  // button was clicked
          buttons[i + MATRIX_COLS * j].buttonPressed = true;
        }
        gpio_output_set(0, 0, 0, 1 << rowPins[j]);  // disable pin output
        gpio_pad_pullup(rowPins[j]);                // Enable pull-up resistor (probably already active, just make sure)
        gpio_pad_input_enable(rowPins[j]);          // Enable input mode
      }
    }
  }
  // note: there is no need to set rowPins to outputs here, they will be initialized in the setup

#else  // normal buttons
  // configure button pins as input and read values
  for (int i = 0; i < NUM_BUTTONS; i++) {
    gpio_pad_unhold(buttonPins[i]);                                 // release from hold
    gpio_pad_select_gpio(buttonPins[i]);                            // Select GPIO function
    gpio_pad_pullup(buttonPins[i]);                                 // Enable pull-up resistor
    gpio_pad_input_enable(buttonPins[i]);                           // Enable input mode
    bool buttonState = (gpio_input_get() >> buttonPins[i]) & BIT0;  // read GPIO state
    if (buttonState == LOW) {
      buttons[i].buttonClicks++;  // button was clicked
      buttons[i].buttonPressed = true;
    }
  }
#endif
}
