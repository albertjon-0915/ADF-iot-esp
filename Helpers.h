#ifndef HELPERS_H
#define HELPERS_H

#include "env.h"
#include "ONOFF.h"
#include <HTTPClient.h>
#include "time.h"
#include <WiFi.h>

// *NOTE: uncomment for esp32 30/38 pin configuration
static constexpr uint8_t L298N_PWM = 13;
static constexpr uint8_t FSR_PIN = 32;
static constexpr uint8_t MOTOR1_PIN = 17;
static constexpr uint8_t MOTOR2_PIN = 16;

// *NOTE: uncomment for esp32c3 super mini pin configuration
// static constexpr uint8_t L298N_PWM = 6;
// static constexpr uint8_t FSR_PIN = 2;
// static constexpr uint8_t MOTOR1_PIN = 4;
// static constexpr uint8_t MOTOR2_PIN = 5;

struct rtdb_data;
struct WEIGHT;


void firebaseInit();                          // call after WiFi connected
void firebasePoll();                          // call regularly from loop()
void rawPolling();                          // not an async polling
void firebaseSendStatus(const rtdb_data &d);  // send status update to RTDB



struct Wifi {
  String ssid;
  String pass;
};

struct rtdb_data {
  String FB_breakfast;
  String FB_lunch;
  String FB_dinner;
  String FB_status;
  double FB_foodAmount;
  bool FB_isFeeding;
};

struct WEIGHT {
  int FSR_PIN;
  int SAMPLES;
  float ADC_noLoad;
  float ADC_wLoad;
  float WEIGHT_noLoad;
  float WEIGHT_wLoad;
  float TARE_offset;
};

enum CON_STATUS { INVALID_CREDS,
                  ERROR,
                  SUCCESS };
enum response { DONE,
                FAILED };

enum MOTOR {
  SLEEP,
  RUNNING,
};

enum STAGE {
  FIRST,
  SECOND,
  FINAL
};

extern rtdb_data jsonResp;
extern rtdb_data DISPENSING;
extern rtdb_data FOODREADY;
extern rtdb_data IDLE;
extern String TIME_now;
extern WEIGHT WEIGHT_Data;
extern MOTOR MOTOR_state;

ONOFF &motorControl_1();
ONOFF &motorControl_2();
CON_STATUS fireBaseConnect();
rtdb_data GET_DATA();
response FB_createLog();
String getCurrentTime();

void SEND_DATA(rtdb_data &data);
void CL_trigger();
void rotateAction();
void stopRotateAction();
void WEIGHT_init();
void UPDATE(STAGE stage);
void CL_runners();
int WEIGHT_read();
float WEIGHT_getGrams();
bool getWiFiStatus();
bool TIME_isFeedNow(rtdb_data &sched);
bool STATUS_isFeedNow(rtdb_data &status);
bool STATUS_isFoodReady(rtdb_data &status);
bool STATUS_isDoneIdle(rtdb_data &status);
bool WEIGHT_isStopFeeding(rtdb_data &food_amount, float current_weight);

#endif  // HELPERS_H
