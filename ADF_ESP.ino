#define DISABLE_DEBUG

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


RTDB_DATA data;
WiFiManager wm;


bool TIME_ISFEED;
bool STATUS_ISFEED;
bool PREVENT_STATUSFEED = false;
bool PREVENT_TIMEFEED = false;
float weight;
FLAG CONTROLLER = INACTIVITY;

// Asynchronous functions without using delays
void assignCurrentTime() {
  TIME_now = getCurrentTime();
  Serial.println(TIME_now);
}

void printResponse() {
  Serial.printf("FIRST   --> %s\n", jsonResp.FB_first);
  Serial.printf("SECOND  --> %s\n", jsonResp.FB_second);
  Serial.printf("THIRD   --> %s\n", jsonResp.FB_third);
  Serial.printf("FOURTH  --> %s\n", jsonResp.FB_fourth);
  Serial.printf("FIFTH   --> %s\n", jsonResp.FB_fifth);
}

CREATE_ASYNC_FN(GET_dateTime, 5000, assignCurrentTime);
CREATE_ASYNC_FN(PRINT_res, 1000, printResponse);


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
  WEIGHT_begin();
  // CL_runners();
}

void loop() {
  indicator();
  asyncDelay(GET_dateTime);
  if (TIME_now == "Readying Time, please wait...") return;

  firebasePoll();

  STATUS_ISFEED = STATUS_isFeedNow(jsonResp);
  TIME_ISFEED = TIME_isFeedNow(jsonResp);


  if (!PREVENT_STATUSFEED && STATUS_ISFEED && CONTROLLER == INACTIVITY) CONTROLLER = INITIAL;
  if (!PREVENT_TIMEFEED && TIME_ISFEED && CONTROLLER == INACTIVITY) {
    PREVENT_STATUSFEED = true;
    PREVENT_TIMEFEED = true;
    CONTROLLER = INITIAL;
  }

  if(!TIME_ISFEED){
    PREVENT_TIMEFEED = false;
  }


  if (CONTROLLER == INITIAL) {
    Serial.println("FIRST STAGE");

    if (TIME_ISFEED) {
      UPDATE(FIRST);
      Serial.println("via TIME: Feed time !!!");
    }

    if (STATUS_ISFEED) Serial.println("via MANUAL: Feeding time !!!");

    rotateAction();
    CONTROLLER = PROCCEED;
  }

  if (CONTROLLER == PROCCEED) {
    Serial.println("SECOND STAGE");
    weight = WEIGHT_getGrams();  // read analog value and convert to grams

    if (WEIGHT_isStopFeeding(jsonResp, weight)) {
      bool cycle = false;
      stopRotateAction();

      while (!cycle) {
        UPDATE(SECOND);  // update to foodready
        delay(2000);
        cycle = STATUS_isFoodReady(jsonResp);
      }

      CONTROLLER = END;
    }
  }

  if (CONTROLLER == END) {
    Serial.println("FINAL STAGE");
    weight = WEIGHT_getGrams();  // read analog value and convert to grams

    if (weight <= 5) {
      bool cycle = false;

      while (!cycle) {
        UPDATE(FINAL);  // update to IDLE
        delay(2000);
        cycle = STATUS_isDoneIdle(jsonResp);
      }

      CL_trigger();
      CONTROLLER = INACTIVITY;
      PREVENT_STATUSFEED = false;
    }
  }

  // asyncDelay(PRINT_res);
  delay(200);
}
