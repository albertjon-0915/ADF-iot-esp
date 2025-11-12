// #define DISABLE_DEBUG

#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#include <Arduino.h>
#include <WiFiManager.h>
#include "env.h"
#include "Helpers.h"
#include "Utils.h"


const char* ntpServer = "pool.ntp.org";
const long GMT = 8 * 3600;  // GMT+8 (Philippines time)
const int DST = 0;          // No DST in PH


const int PWM_channel = 0;
const int PWM_freq = 5000;
const int PWM_resolution = 8;

rtdb_data data;
WiFiManager wm;

bool FLAG_feed = false;
bool FLAG_stop = false;

// WEIGHT WEIGHT_Data = {
//   .FSR_PIN = LM393_CPM,
//   .SAMPLES = 20,
//   .ADC_noLoad = 4095,
//   .ADC_wLoad = 1100.0,
//   .WEIGHT_noLoad = 0.0,
//   .WEIGHT_wLoad = 400.0
// };

void assignCurrentTime() {
  TIME_now = getCurrentTime();
  Serial.println(TIME_now);
}

void updateRtdbDispensing() {
  firebaseSendStatus(DISPENSING);
}
void updateRtdbFReady() {
  firebaseSendStatus(FOODREADY);
}
void updateRtdbIdle() {
  firebaseSendStatus(IDLE);
}

CREATE_ASYNC_FN(GET_dateTime, 5000, assignCurrentTime);
CREATE_ASYNC_FN(POST_Dispensing, 12000, updateRtdbDispensing);
CREATE_ASYNC_FN(POST_FReady, 8000, updateRtdbFReady);
CREATE_ASYNC_FN(POST_Idle, 12000, updateRtdbIdle);

void setup() {
  Serial.begin(115200);
  delay(1000);

  ledcAttachChannel(L298N_PWM, PWM_freq, PWM_resolution, PWM_channel);

  configTime(GMT, DST, ntpServer);

  WiFi.mode(WIFI_MODE_STA);            //  STA mode (explicit call -> will deafult to AP+STA anyway)
  bool res = wm.autoConnect("espGo");  // anonymous ap

  if (!res) Serial.println("Failed to connect");
  else Serial.println("Connected to network !!!");

  firebaseInit();
}

void loop() {
  asyncDelay(GET_dateTime);

  ledcWrite(PWM_channel, 155);
  float weight = WEIGHT_getGrams();  // read analog value and convert to grams

  firebasePoll();

  if (TIME_isFeedNow(jsonResp)) {
    FLAG_feed = true;
    Serial.println("via TIME: Feed time !!!");
  }

  if (STATUS_isFeedNow(jsonResp)) {
    FLAG_feed = true;
    Serial.println("via MANUAL: Feeding time !!!");
  }

  while (FLAG_feed == true && FLAG_stop == false) {
    asyncDelay(POST_Dispensing);
    rotateAction();
    weight = WEIGHT_getGrams();
    yield();  // add a little pause for background tasks

    if (WEIGHT_isStopFeeding(data, weight)) {
      FLAG_stop = true;
      Serial.println("Stopping feed: Target reached");
      firebaseSendStatus(FOODREADY);
      stopRotateAction();
      CL_trigger();
      FLAG_feed = false;
      return;
    }
  }

  // if the food is already eaten by the pet, update status
  if (weight < 100) {
    FLAG_stop = false;
    asyncDelay(POST_Idle);
  }
}
