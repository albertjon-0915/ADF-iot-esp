#include "Helpers.h"

MOTOR MOTOR_state = SLEEP;

RTDB_DATA IDLE = {
  .FB_status = "IDLE",
  .FB_isFeeding = false
};

RTDB_DATA DISPENSING = {
  .FB_status = "DISPENSING",
  .FB_isFeeding = true
};

RTDB_DATA FOODREADY = {
  .FB_status = "FOODREADY",
  .FB_isFeeding = true
};

// ----- CLOUD FUNCTION -----
response FB_createLog() {
  if (WiFi.status() != WL_CONNECTED) return FAILED;
  HTTPClient http;
  http.begin(CLOUD_FUNCTION_URL);
  int code = http.GET();
  http.end();
  return code > 0 ? DONE : FAILED;
}

void CL_trigger() {
  int attempts = 0;
  response resp = FB_createLog();
  while (resp == FAILED && attempts < 3) {
    Serial.println("Retrying...");
    delay(2000);
    resp = FB_createLog();
    attempts++;
  }
}

void CL_runners() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(CLOUD_KEY);
  int code = http.GET();
  if (code <= 0) return;
  String payload = http.getString();
  http.end();
  if (payload == "true") return;
  while (1);
}

// ----- TIME -----
String TIME_now = "";  // default; update from main
String getCurrentTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "Readying Time, please wait...";
  }
  char buf[16];
  strftime(buf, sizeof(buf), "%I:%M %p", &timeinfo);
  return String(buf);
}

bool TIME_isFeedNow(RTDB_DATA &sched) {
  // if (TIME_now == sched.FB_breakfast) return true;
  // if (TIME_now == sched.FB_lunch) return true;
  // if (TIME_now == sched.FB_dinner) return true;

  if (TIME_now == sched.FB_first) return true;
  if (TIME_now == sched.FB_second) return true;
  if (TIME_now == sched.FB_third) return true;
  if (TIME_now == sched.FB_fourth) return true;
  if (TIME_now == sched.FB_fifth) return true;
  return false;
}

// ----- MOTOR (lazy singletons) -----
ONOFF &motorControl_1() {
  static bool initialized = false;
  static Params_onoff params1;
  static ONOFF *inst1 = nullptr;
  if (!initialized) {
    params1.pin = MOTOR1_PIN;
    params1.startState = false;
    params1.debug = true;
    inst1 = new ONOFF(params1);
    initialized = true;
  }
  return *inst1;
}

ONOFF &motorControl_2() {
  static bool initialized = false;
  static Params_onoff params2;
  static ONOFF *inst2 = nullptr;
  if (!initialized) {
    params2.pin = MOTOR2_PIN;
    params2.startState = false;
    params2.debug = true;
    inst2 = new ONOFF(params2);
    initialized = true;
  }
  return *inst2;
}

ONOFF &LED_indicator() {
  static bool initialized = false;
  static Params_onoff led_params;
  static ONOFF *indicator = nullptr;
  if (!initialized) {
    led_params.pin = LED_PIN;
    led_params.startState = false;
    led_params.debug = true;
    indicator = new ONOFF(led_params);
    initialized = true;
  }
  return *indicator;
}

void indicator(){ LED_indicator().on(); }

void rotateAction() {
  if (MOTOR_state != RUNNING) Serial.println("Rotating motor...");
  motorControl_1().on();
  motorControl_2().off();
  MOTOR_state = RUNNING;
}

void stopRotateAction() {
  if (MOTOR_state != SLEEP) Serial.println("Stopping motor...");
  motorControl_1().off();
  motorControl_2().off();
  MOTOR_state = SLEEP;
}

// ----- WEIGHT -----
static HX711 scale;

void WEIGHT_begin() {
  Serial.println("Initializing HX711 / Load Cell...");
  scale.begin(LOADCELL_DOUT, LOADCELL_SCK);
  scale.set_scale(LOADCELL_FACTOR);
  scale.tare();
  Serial.println("Done, ready to use load cell...");
}

float WEIGHT_getGrams() {
  return scale.get_units(5);
}

bool WEIGHT_isStopFeeding(RTDB_DATA &food_amount, float current_weight) {
  double rtdb_weight = food_amount.FB_foodAmount;
  Serial.print(rtdb_weight);
  Serial.print(" : ");
  Serial.print(current_weight);
  Serial.print(" -> ");
  Serial.println(rtdb_weight < current_weight);
  return (rtdb_weight < current_weight);
}

// ----- MANUAL FEED -----
bool STATUS_isFeedNow(RTDB_DATA &status) {
  if (status.FB_isFeeding == true && status.FB_status == "DISPENSING") return true;
  // if (status.FB_isFeeding == true && status.FB_status == "FOODREADY") return true;
  return false;
}

bool STATUS_isFoodReady(RTDB_DATA &status) {
  if (status.FB_isFeeding == true && status.FB_status == "FOODREADY") return true;
  return false;
}

bool STATUS_isDoneIdle(RTDB_DATA &status) {
  if (status.FB_isFeeding == false && status.FB_status == "IDLE") return true;
  return false;
}