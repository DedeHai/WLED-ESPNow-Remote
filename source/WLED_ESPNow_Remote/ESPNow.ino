/**********************************************************************
  ESPNow remote for WLED
  Copyright (c) 2024  Damian Schneider (@DedeHai)
  Licensed under the EUPL v. 1.2 or later
  https://github.com/DedeHai/WLED-ESPNow-Remote
***********************************************************************/


#define WIZMOTE_BUTTON_ON 1
#define WIZMOTE_BUTTON_OFF 2
#define WIZMOTE_BUTTON_NIGHT 3
#define WIZMOTE_BUTTON_BRIGHT_DOWN 8  
#define WIZMOTE_BUTTON_BRIGHT_UP 9    
#define WIZMOTE_BUTTON_ONE 16         
#define WIZMOTE_BUTTON_TWO 17       
#define WIZMOTE_BUTTON_THREE 18       
#define WIZMOTE_BUTTON_FOUR 19        

#define WIZ_SMART_BUTTON_ON 100
#define WIZ_SMART_BUTTON_OFF 101
#define WIZ_SMART_BUTTON_BRIGHT_UP 102
#define WIZ_SMART_BUTTON_BRIGHT_DOWN 103

#define REPEATSEND 1       // how many times the message is repeated (increase in very noisy environments, usually 1 is enough)
#define SENDTIMEOUT 10000  // set 10ms timout, sendouts usually take less than 1ms, up to 6ms if there is traffic


// message structure used by WiZmote, only sequence number and button code are used by WLED but message structure needs to match
typedef struct WizMoteMessageStructure {
  uint8_t program;        // 0x91 for ON button, 0x81 for all others
  uint8_t seq[4];         // Incremetal sequence number 32 bit unsigned integer LSB first
  uint8_t dt1 = 0x20;     // Button Data Type (0x20)
  uint8_t button;         // Identifies which button is being pressed
  uint8_t dt2 = 0x01;     // Battery Level Data Type (0x01)
  uint8_t batLevel = 90;  // Battery Level 0-100

  uint8_t byte10;  // Unknown, maybe checksum
  uint8_t byte11;  // Unknown, maybe checksum
  uint8_t byte12;  // Unknown, maybe checksum
  uint8_t byte13;  // Unknown, maybe checksum
} message_structure_t;

RTC_DATA_ATTR unsigned wizCounter = 0;  //store counter in RTC memory to persist between sleep and wake-up
message_structure_t outmessage;
bool isSent;  // track message sending
const uint8_t globalBroadcastAddress[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

// callback function: executed after data sending is done, used to track when the next message can be sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  isSent = true;
}

bool initEspNow() {
  WiFi.mode(WIFI_STA);                             // enable wifi in STA mode
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);  // start out on channel 1
  WiFi.setSleep(true);                             // should be on by default, just make sure...
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);              // enable auto-sleep of wifi peripheral (keeps wifi off after wake from sleep)
  WiFi.setTxPower(WIFI_POWER_15dBm);               // set TX power level (higher levels have higher range but may crash the C3 due to high current demand)
  if (esp_now_init() != ESP_OK) {
    blink(20);  // indicate error on LED
    esp_sleep_enable_timer_wakeup(10000000);
    esp_deep_sleep_start();  // go to deep sleep for 10 seconds to save some battery in case this is a boot-loop
  }
  esp_now_register_send_cb(esp_now_send_cb_t(OnDataSent));  // register callback function
  //esp_now_register_recv_cb(esp_now_recv_cb_t(onDataReceive)); // receive is not used in broadcast
  esp_now_peer_info_t peerInfo = {};                      // register peer (i.e. a broadcast)
  memcpy(peerInfo.peer_addr, globalBroadcastAddress, 6);  // use centered channel as default (in case there is antenna calibration going on, probably unimportant)
  peerInfo.channel = 0;                                   // use 0 if not using a specific channel
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);
  esp_now_set_wake_window(1);  // setting a short listening interval may help to save some power

  return true;
}

void broadcastAcrossChannels() {
  isSent = false;  // start with a timout on first call
  for (int i = 1; i < 14; i++) {
    esp_wifi_set_channel(i, WIFI_SECOND_CHAN_NONE);
    for (int s = 0; s < REPEATSEND; s++) {
      // delayMicroseconds(50);  // short delay helps in case there is traffic on the channel -> uncomment if using multiple sends
      esp_err_t result = esp_now_send(globalBroadcastAddress, (uint8_t *)&outmessage, sizeof(outmessage));
      if (result != ESP_OK) {
        // if sending fails, all subsequend sends fail as well until resetting wifi (error ESP_ERR_ESPNOW_NO_MEM) restarting wifi takes 5s in this case, so better reboot and be ready again much faster
        if (result == ESP_ERR_ESPNOW_NO_MEM) {
          esp_sleep_enable_timer_wakeup(1000);
          esp_deep_sleep_start();  // sleep for 1ms, then reboot
        }
      }
      isSent = false;
      uint32_t timeout = esp_timer_get_time() + SENDTIMEOUT;  // 10ms timeout
      bool oneshot = true; // read buttons once per channel (i.e. at least once every SENDTIMEOUT = 10ms)
      while (esp_timer_get_time() < timeout && isSent == false) {  // wait for msg to be sent
        if(oneshot) {
          handleButtons();  // poll buttons
          oneshot = false;
        }
#ifdef ROTARY_ENCODER
        handleRotaryEncoder(true);  // poll rotary encoder, skip DT reading: interrupt reads pin state and sets timestamp (rotary encoder uses interrupt (+ polling) as changes can be as fast as 0.3ms and sendout blocks for about ~1ms)
#endif
        
      }
    }
  }
}

void sendPress(uint8_t buttonCode) {
  esp_wifi_start();  // enable wifi peripheral
  wizCounter++;      // increment message sequence counter
  outmessage.seq[0] = byte(wizCounter);
  outmessage.seq[1] = byte(wizCounter >> 8);
  outmessage.seq[2] = byte(wizCounter >> 16);
  outmessage.seq[3] = byte(wizCounter >> 24);
  outmessage.button = buttonCode;
  outmessage.program = 0x81;
  if (buttonCode == WIZMOTE_BUTTON_ON)
    outmessage.program = 0x91;  // 0x91 for ON button, 0x81 for all others

  broadcastAcrossChannels();
  esp_wifi_stop();  // enable wifi peripheral
}
