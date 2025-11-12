#include "Helpers.h"



rtdb_data IDLE = {
  .FB_status = "IDLE",
  .FB_isFeeding = false
};

rtdb_data DISPENSING = {
  .FB_status = "DISPENSING",
  .FB_isFeeding = true
};

rtdb_data FOODREADY = {
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

// ----- TIME -----
String TIME_now = "";  // default; update from main
String getCurrentTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
  }
  char buf[16];
  strftime(buf, sizeof(buf), "%I:%M %p", &timeinfo);
  return String(buf);
}

bool TIME_isFeedNow(rtdb_data &sched) {
  if (TIME_now == sched.FB_breakfast) return true;
  if (TIME_now == sched.FB_lunch) return true;
  if (TIME_now == sched.FB_dinner) return true;
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

void rotateAction() {
  Serial.println("Rotating motor...");
  motorControl_1().on();
  motorControl_2().off();
}

void stopRotateAction() {
  Serial.println("Stopping motor...");
  motorControl_1().off();
  motorControl_2().off();
}

// ----- WEIGHT -----
WEIGHT WEIGHT_Data = {
  .FSR_PIN = FSR_PIN,
  .SAMPLES = 20,
  .ADC_noLoad = 4095.0,
  .ADC_wLoad = 1100.0,
  .WEIGHT_noLoad = 0.0,
  .WEIGHT_wLoad = 400.0,
  .TARE_offset = 0.0
};

float slope = 1.0;

void WEIGHT_init() {
  slope = (WEIGHT_Data.WEIGHT_wLoad - WEIGHT_Data.WEIGHT_noLoad) / (WEIGHT_Data.ADC_wLoad - WEIGHT_Data.ADC_noLoad);
}

int WEIGHT_read() {
  long sum = 0;
  for (int i = 0; i < WEIGHT_Data.SAMPLES; ++i) {
    sum += analogRead(WEIGHT_Data.FSR_PIN);
    delay(5);
  }
  return sum / WEIGHT_Data.SAMPLES;
}

float WEIGHT_getGrams() {
  int raw = WEIGHT_read();
  float grams = slope * (raw - WEIGHT_Data.ADC_noLoad) + WEIGHT_Data.WEIGHT_noLoad;
  grams -= WEIGHT_Data.TARE_offset;
  if (grams < 10.0) grams = 0.0;
  else {
    Serial.print("ADC: ");
    Serial.print(raw);
    Serial.print("  →  Weight (g): ");
    Serial.println(grams, 1);
  }
  return grams;
}

bool WEIGHT_isStopFeeding(rtdb_data &food_amount, float current_weight) {
  double rtdb_weight = food_amount.FB_foodAmount;
  return (rtdb_weight < current_weight);
}