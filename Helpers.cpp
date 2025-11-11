#include "Helpers.h"

// ----- globals -----
// removed Preferences prefs; persistence removed

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

// ----- WIFI -----
bool getWiFiStatus() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("STA IP: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("\nNo STA WiFi or failed connection.");
    delay(100);
    return false;
  }
}

// // ----- FIREBASE -----
// ssl_client.setInsecure();
// ssl_client.setConnectionTimeout(1000);
// ssl_client.setHandshakeTimeout(5);
// initializeApp(aClient, app, getAuth(user_auth), processData, "🔐 authTask");
// app.getApp<RealtimeDatabase>(Database);
// Database.url(FIREBASE_DB_URL);


// rtdb_data GET_DATA() {
//   rtdb_data jsonResp = { "", "", "", "", 0.0, false };
//   Database.get(aClient, "/feeder_status/breakfast_sched", GET_processData, false, "BF");
//   Database.get(aClient, "/feeder_status/lunch_sched", GET_processData, false, "LH");
//   Database.get(aClient, "/feeder_status/dinner_sched", GET_processData, false, "DR");
//   Database.get(aClient, "/feeder_status/feeding_status", GET_processData, false, "FS");
//   Database.get(aClient, "/feeder_status/feed_amount", GET_processData, false, "FA");
//   Database.get(aClient, "/feeder_status/isFeeding", GET_processData, false, "IF");

//   void GET_processData() {
//     if (!aResult.isResult())
//       return;

//     if (aResult.isEvent())
//       Firebase.printf("Event task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.eventLog().message().c_str(), aResult.eventLog().code());
//     if (aResult.isDebug())
//       Firebase.printf("Debug task: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());
//     if (aResult.isError())
//       Firebase.printf("Error task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.error().message().c_str(), aResult.error().code());

//     if (aResult.available()) {
//       Firebase.printf("task: %s, payload: %s\n", aResult.uid().c_str(), aResult.c_str());

//       String payload = aResult.c_str();

//       switch (aResult.uid()) {
//         case 'BF':  // BF
//           jsonResp.FB_breakfast = payload;
//           break;
//         case 'LH':  // LH
//           jsonResp.FB_lunch = payload;
//           break;
//         case 'DR':  // DR
//           jsonResp.FB_dinner = payload;
//           break;
//         case 'FS':  // FS
//           jsonResp.FB_status = payload;
//           break;
//         case 'FA':  // FA
//           jsonResp.FB_foodAmount = payload.toDouble();
//           break;
//         case 'IF':  // IF
//           jsonResp.FB_isFeeding = (payload == "true");
//           break;
//       }
//     }
//   }

//   return jsonResp;
// }

// void SEND_DATA(rtdb_data &data) {
//   Database.set<String>(aClient, "/feeder_status/feeding_status", data.FB_status, processData, "US");
//   Database.set<bool>(aClient, "/feeder_status/isFeeding", data.FB_isFeeding, processData, "UI");
// }

//   void processData(AsyncResult & aResult) {
//     if (!aResult.isResult())
//       return;

//     if (aResult.isEvent())
//       Firebase.printf("Event task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.eventLog().message().c_str(), aResult.eventLog().code());
//     if (aResult.isDebug())
//       Firebase.printf("Debug task: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());
//     if (aResult.isError())
//       Firebase.printf("Error task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.error().message().c_str(), aResult.error().code());
//     if (aResult.available())
//       Firebase.printf("task: %s, payload: %s\n", aResult.uid().c_str(), aResult.c_str());
//   }

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
    params1.pin = 4; // 4
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
    params2.pin = 5; // 5
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
  .FSR_PIN = 2, //36
  .SAMPLES = 20,
  .ADC_noLoad = 0.0,
  .ADC_wLoad = 0.0,
  .WEIGHT_noLoad = 0.0,
  .WEIGHT_wLoad = 0.0,
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
  if (grams < 0.0) grams = 0.0;
  Serial.print("ADC: ");
  Serial.print(raw);
  Serial.print("  →  Weight (g): ");
  Serial.println(grams, 1);
  return grams;
}

bool WEIGHT_isStopFeeding(rtdb_data &food_amount, float current_weight) {
  double rtdb_weight = food_amount.FB_foodAmount;
  return (rtdb_weight < current_weight);
}