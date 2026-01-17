#ifndef HELPERS_H
#define HELPERS_H

#include "env.h"
#include "ONOFF.h"
#include <HTTPClient.h>
#include "time.h"
#include <WiFi.h>
#include "HX711.h"


// *NOTE: uncomment for esp32 30/38 pin configuration
static constexpr uint8_t L298N_PWM = 13;
static constexpr uint8_t FSR_PIN = 32;
static constexpr uint8_t MOTOR1_PIN = 17;
static constexpr uint8_t MOTOR2_PIN = 16;
static constexpr uint8_t LOADCELL_DOUT = 34;
static constexpr uint8_t LOADCELL_SCK = 4;
static constexpr uint8_t LED_PIN = 18;
static constexpr float LOADCELL_FACTOR = 705.011;


struct RTDB_DATA;
struct WEIGHT;

void firebaseInit();                          // call after WiFi connected
void firebasePoll();                          // call regularly from loop()
void firebaseSendStatus(const RTDB_DATA &d);  // send status update to RTDB

struct RTDB_DATA {
  String FB_first;
  String FB_second;
  String FB_third;
  String FB_fourth;
  String FB_fifth;
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

enum FLAG {
  INACTIVITY,
  INITIAL,
  PROCCEED,
  END
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

extern RTDB_DATA jsonResp;
extern RTDB_DATA DISPENSING;
extern RTDB_DATA FOODREADY;
extern RTDB_DATA IDLE;
extern String TIME_now;
extern MOTOR MOTOR_state;

ONOFF &motorControl_1();
ONOFF &motorControl_2();
CON_STATUS fireBaseConnect();
RTDB_DATA GET_DATA();
response FB_createLog();
String getCurrentTime();

void SEND_DATA(RTDB_DATA &data);
void CL_trigger();
void rotateAction();
void stopRotateAction();
void WEIGHT_begin();
void UPDATE(STAGE stage);
void CL_runners();
void indicator();
bool getWiFiStatus();
bool TIME_isFeedNow(RTDB_DATA &sched);
bool STATUS_isFeedNow(RTDB_DATA &status);
bool STATUS_isFoodReady(RTDB_DATA &status);
bool STATUS_isDoneIdle(RTDB_DATA &status);
bool WEIGHT_isStopFeeding(RTDB_DATA &food_amount, float current_weight);
float WEIGHT_getGrams();

#endif  // HELPERS_H
