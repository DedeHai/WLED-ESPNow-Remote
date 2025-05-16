/**********************************************************************
  
  ESPNow based remote controller for WLED using ESP32 C3
  with support for buttons, button matrix and rotary encoder

  Copyright (c) 2024  Damian Schneider (@DedeHai)
  Licensed under the EUPL v. 1.2 or later
  https://github.com/DedeHai/WLED-ESPNow-Remote

  Configuration:
   - hardware settings: config.h
   - button function assignment: buttons.ino
   - more info is available in the project readme

***********************************************************************/

#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include "config.h"
#include "globals.h"


/////////////// SETUP /////////////////
void setup() {
  setCpuFrequencyMhz(80);  // reduce clock to minimum supported by wifi peripheral to save power
  pinMode(LED_PIN, OUTPUT);
  LED_OFF;  

  initButtons();    // initialize inputs, update states

#ifdef ROTARY_ENCODER
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {  // timer wakeup from deep sleep, reset rotary level and go to sleep for good
    rotaryLevel = 0;
    enterDeepSleep(0);  // calling with a time of 0 will go to sleep indefinitely
  }
  // Inputs
  pinMode(CLK_PIN, INPUT_PULLUP);
  pinMode(DT_PIN, INPUT_PULLUP);
#endif

  initEspNow();     // 45ms
  esp_wifi_stop();  // disable wifi until sendout to save power (wifi on: 23mA, wifi off: 0.3mA)

  //TODO: need to handle infinite button press gracefully, maybe go to deep sleep for 1s after a time limit of 1 minute? it will still drain the battery due to pullup but not as fast
}

/////////////// MAIN LOOP /////////////////
void loop() {
  static uint32_t lastsendtime = 0;
  uint32_t sleeptime = 1000000;  // default sleep time while active is 1s, shorter if there is messages queued or button action going on note: this could be increased but it saves little additional power, polling once per second ensures status stays correct in case of missed pulses

  uint32_t now = esp_timer_get_time();
  if (now - lastButtonEdgeTime < DOUBLE_CLICK_TIME * 1000) sleeptime = BUTTONPOLLINTERVAL;  // poll buttons if there was any changes recently
  else if (anyButtonPressed()) sleeptime = LONG_PRESS_TIME / 3 * 1000;                      // check for long press (200ms default) after double-click window has passed
  else if (!buffer_is_empty()) sleeptime = MINSENDINTERVAL;                                 // sleep for sendout interval until buffer is empty

  // monitor the inputs until an action is queued
  sleepAndMonitor(sleeptime, false);  // runs input handlers on GPIO wakeup, polls button state when waking up

  if (esp_timer_get_time() - lastsendtime > MINSENDINTERVAL) {  // too fast sending can cause trouble: overloaded clients, overloaded queue, both result in unresponsive sends, full wifi peripheral reset is possible but slow
    uint8_t command = 0;
    if (buffer_read(command)) {
       LED_ON;  
#ifdef ROTARY_ENCODER
      attachInterrupt(digitalPinToInterrupt(CLK_PIN), clk_pin_isr, CHANGE);  // enable pin interrupts during sendout
#endif
      if (command > 0) {
        sendPress(command);  // send command through ESPNow, one send takes xxx ms
        lastsendtime = esp_timer_get_time();
      }
#ifdef ROTARY_ENCODER
      detachInterrupt(digitalPinToInterrupt(CLK_PIN));  // disable pin interrupts: interrupt during sleep can causes continuous firing of ISR, even if flags are cleared manually, so use polling at wakeup
#endif
       LED_OFF;
    }
  }
#ifndef DISABLE_DEEPSLEEP // enter deep sleep if not disabled
  if (esp_timer_get_time() - lastsendtime > ACTIVE_TIME * 1000UL) {
#ifdef ROTARY_ENCODER
    enterDeepSleep(KEEPROTARYLEVEL_TIME * 1000000UL);
#else
    enterDeepSleep(0);  // go to deep sleep indefinitely
#endif
  }
#endif
}
