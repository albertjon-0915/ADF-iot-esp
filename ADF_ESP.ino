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
bool FLAG_update = false;
bool FLAG_complete = false;
bool FLAG_lock = false;

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
  // Serial.println(TIME_now);
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
CREATE_ASYNC_FN(GET_Poll, 5000, firebasePoll);
// CREATE_ASYNC_FN(POST_Dispensing, 12000, updateRtdbDispensing);
// CREATE_ASYNC_FN(POST_FReady, 8000, updateRtdbFReady);
// CREATE_ASYNC_FN(POST_Idle, 12000, updateRtdbIdle);

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
  ledcWrite(PWM_channel, 255);

  asyncDelay(GET_dateTime);
  asyncDelay(GET_Poll);
  // firebasePoll();


  bool TIME_ISFEED;
  bool STATUS_ISFEED;
  float weight = WEIGHT_getGrams();  // read analog value and convert to grams
  STATUS_ISFEED = STATUS_isFeedNow(jsonResp);
  TIME_ISFEED = TIME_isFeedNow(jsonResp);


  if (TIME_ISFEED || STATUS_ISFEED && !FLAG_lock) FLAG_feed = true;
  if (TIME_ISFEED && !FLAG_update) { FLAG_update = true; firebaseSendStatus(DISPENSING); }


  if (FLAG_feed == true) {
    Serial.println("FIRST STAGE");

    if (TIME_ISFEED) Serial.println("via TIME: Feed time !!!");
    if (STATUS_ISFEED) Serial.println("via MANUAL: Feeding time !!!");

    rotateAction();
    FLAG_feed = false;  // close the 1st stage
    FLAG_stop = true;   // unlock the 2nd stage
    FLAG_lock = true;   // finish cycle before feeding again
  }

  if (FLAG_stop == true) {
    Serial.println("SECOND STAGE");

    if (WEIGHT_isStopFeeding(jsonResp, weight)) {
      stopRotateAction();
      FLAG_stop = false;              // close the 2nd stage
      FLAG_complete = true;           //  unlock the final stage
      firebaseSendStatus(FOODREADY);  // update to foodready
    }
    return;
  }

  if (FLAG_complete == true) {
    Serial.println("FINAL STAGE");

    if (weight <= 100) {
      FLAG_update = false;       // lift the update lock on dispensing
      FLAG_complete = false;     // close the final stage
      FLAG_lock = false;         // release the cycle lock
      firebaseSendStatus(IDLE);  // update to IDLE
    }
    return;
  }

  delay(50);
}
