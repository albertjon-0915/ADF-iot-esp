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
const int PWM_freq = 30000;
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
  Serial.println(TIME_now);
}

// void updateRtdbDispensing() {
//   firebaseSendStatus(DISPENSING);
// }

// void updateRtdbFReady() {
//   firebaseSendStatus(FOODREADY);
// }

// void updateRtdbIdle() {
//   firebaseSendStatus(IDLE);
// }

void polling() {
  firebasePoll();
}

CREATE_ASYNC_FN(GET_dateTime, 5000, assignCurrentTime);
CREATE_ASYNC_FN(FR_polling, 10000, polling);


void setup() {
  Serial.begin(115200);
  ledcAttachChannel(L298N_PWM, PWM_freq, PWM_resolution, PWM_channel);
  ledcWriteChannel(PWM_channel, 255);

  configTime(GMT, DST, ntpServer);

  WiFi.mode(WIFI_MODE_STA);            //  STA mode (explicit call -> will default to AP+STA anyway)
  bool res = wm.autoConnect("espGo");  // anonymous ap

  if (!res) Serial.println("Failed to connect");
  else Serial.println("Connected to network !!!");

  WiFi.setSleep(true);

  firebaseInit();
  CL_runners();
}

void loop() {
  asyncDelay(GET_dateTime);
  if(TIME_now == "Readying Time, please wait...") return;

  bool TIME_ISFEED;
  bool STATUS_ISFEED;
  float weight;
  STATUS_ISFEED = STATUS_isFeedNow(jsonResp);
  TIME_ISFEED = TIME_isFeedNow(jsonResp);

  // firebasePoll();
  asyncDelay(FR_polling);
  Serial.println("polling feeding time confirmation...");

  if (TIME_ISFEED || STATUS_ISFEED && !FLAG_lock) FLAG_feed = true;
  if (TIME_ISFEED && !FLAG_update) {
    FLAG_update = true;
    UPDATE(FIRST);
  }


  if (FLAG_feed) {
    Serial.println("FIRST STAGE");

    if (TIME_ISFEED) Serial.println("via TIME: Feed time !!!");
    if (STATUS_ISFEED) Serial.println("via MANUAL: Feeding time !!!");

    rotateAction();
    FLAG_feed = false;  // close the 1st stage
    FLAG_stop = true;   // unlock the 2nd stage
    FLAG_lock = true;   // finish cycle before feeding again
  }

  if (FLAG_stop) {
    Serial.println("SECOND STAGE");
    weight = WEIGHT_getGrams();  // read analog value and convert to grams

    if (WEIGHT_isStopFeeding(jsonResp, weight)) {
      // firebaseSendStatus(FOODREADY);  // update to foodready
      stopRotateAction();
      FLAG_stop = false;     // close the 2nd stage
      FLAG_complete = true;  //  unlock the final stage
      Serial.println("polling for foodready confirmation...");
      bool FLAG_escapsulated = false;
      
      do {
        rawPolling();
        UPDATE(SECOND);  // update to foodready
        delay(2000);
        bool FLAG_escapsulated = STATUS_isFoodReady(jsonResp);
      } while (!FLAG_escapsulated);
      Serial.print("food ready confirmed...");
    }
    return;
  }

  if (FLAG_complete) {
    Serial.println("FINAL STAGE");
    weight = WEIGHT_getGrams();  // read analog value and convert to grams

    if (weight <= 100) {
      // firebaseSendStatus(IDLE);  // update to IDLE
      FLAG_update = false;    // lift the update lock on dispensing
      FLAG_complete = false;  // close the final stage
      FLAG_lock = false;      // release the cycle lock
      Serial.println("polling for idle confirmation...");
      bool FLAG_escapsulated = false;

      do {
        rawPolling();
        UPDATE(FINAL);  // update to IDLE
        delay(2000);
        FLAG_escapsulated = STATUS_isDoneIdle(jsonResp);
      } while (!FLAG_escapsulated);
      Serial.print("idle confirmed...");
    }
    Serial.println("feeding done!...");
  }


  delay(200);
}
