#ifndef HELPERS_H
#define HELPERS_H

#include "env.h"
#include <Preferences.h>
#include "esp_wifi.h"
#include "ONOFF.h"  // include library on sketch (https://github.com/albertjon-0915/ON_OFF)
#include <HTTPClient.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include "time.h"
#include <ArduinoJson.h>



// =========================================================================================================
// ========================================== WIFI CONNECTION ==============================================
// =========================================================================================================

Preferences prefs;

struct Wifi {
  String ssid;
  String pass;
};

inline Wifi getLocalWifi() {
  Wifi local;

  prefs.begin("wifi", true);
  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  prefs.end();

  local.ssid = ssid;
  local.pass = pass;
  return local;
}

inline void localSaveCreds(Wifi wifi) {
  if (wifi.ssid.isEmpty()) { return; }
  prefs.begin("wifi", false);
  if (!wifi.ssid.isEmpty()) { prefs.putString("ssid", wifi.ssid); }
  if (!wifi.pass.isEmpty()) { prefs.putString("pass", wifi.pass); }
  prefs.end();
  Serial.println("WiFi credentials saved.");
}

inline void startSmartConfig() {
  Wifi wifi;
  Serial.println("Starting SmartConfig...");
  WiFi.beginSmartConfig();

  while (!WiFi.smartConfigDone()) {
    delay(500);
    Serial.println(".");
  }

  Serial.println("\nSmartConfig received.");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");
  Serial.print("STA IP: ");
  Serial.println(WiFi.localIP());

  // Grab SSID and password from ESP32 Wi-Fi config
  wifi_config_t wifi_config;
  esp_wifi_get_config(WIFI_IF_STA, &wifi_config);

  wifi.ssid = String((char *)wifi_config.sta.ssid);
  wifi.pass = String((char *)wifi_config.sta.password);
  localSaveCreds(wifi);

  WiFi.stopSmartConfig();  // stop listening until requested again
}

inline bool getWiFiStatus() {

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("STA IP: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("\nNo STA WiFi or failed connection, waiting for reset via AP...");
    delay(100);
    return false;
  }
}

// =========================================================================================================
// ========================================== WIFI CONNECTION ==============================================
// =========================================================================================================


// =========================================================================================================
// ======================================== FIREBASE CONNECTION ============================================
// =========================================================================================================

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

enum CON_STATUS {
  INVALID_CREDS,
  ERROR,
  SUCCESS
};

inline bool is_valid_config() {
  if (!FIREBASE_KEY || FIREBASE_KEY[0] == '\0') { return false; }
  if (!FIREBASE_DB_URL || FIREBASE_DB_URL[0] == '\0') { return false; }
  return true;
}

inline CON_STATUS fireBaseConnect() {
  // Firebase credentials
  if (!is_valid_config()) { return INVALID_CREDS; }
  config.api_key = FIREBASE_KEY;
  config.database_url = FIREBASE_DB_URL;

  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Sign up, OK!");
    // signUpOk = true;
    return SUCCESS;
  }

  Serial.printf("%s\n", config.signer.signupError.message.c_str());
  return ERROR;
}

struct rtdb_data {
  String FB_breakfast;
  String FB_lunch;
  String FB_dinner;
  String FB_status;
  double FB_foodAmount;
  bool FB_isFeeding;
};

inline rtdb_data GET_DATA() {
  rtdb_data jsonResp = { "", "", "", "", 0.0, false };  // default

  if (Firebase.ready() && fireBaseConnect() == SUCCESS) {
    String path = "/feeder_status";

    String feedingStatus;
    double foodAmount;
    bool isFeeding;
    String breakfast, lunch, dinner;

    // Read values directly from RTDB
    if (Firebase.RTDB.getString(&fbdo, path + "/feeding_status")) {
      feedingStatus = fbdo.stringData();
    } else {
      Serial.println("Failed to get feeding_status: " + fbdo.errorReason());
    }

    if (Firebase.RTDB.getDouble(&fbdo, path + "/food_amount")) {
      foodAmount = fbdo.doubleData();
    } else {
      Serial.println("Failed to get food_amount: " + fbdo.errorReason());
    }

    if (Firebase.RTDB.getBool(&fbdo, path + "/isFeeding")) {
      isFeeding = fbdo.boolData();
    } else {
      Serial.println("Failed to get isFeeding: " + fbdo.errorReason());
    }

    if (Firebase.RTDB.getString(&fbdo, path + "/breakfast_sched")) {
      breakfast = fbdo.stringData();
    }
    if (Firebase.RTDB.getString(&fbdo, path + "/lunch_sched")) {
      lunch = fbdo.stringData();
    }
    if (Firebase.RTDB.getString(&fbdo, path + "/dinner_sched")) {
      dinner = fbdo.stringData();
    }

    // Fill struct
    jsonResp.FB_status = feedingStatus;
    jsonResp.FB_foodAmount = foodAmount;
    jsonResp.FB_isFeeding = isFeeding;
    jsonResp.FB_breakfast = breakfast;
    jsonResp.FB_lunch = lunch;
    jsonResp.FB_dinner = dinner;
  }

  return jsonResp;
}

inline void SEND_DATA(rtdb_data &data) {
  if (Firebase.ready() && fireBaseConnect() == SUCCESS) {
    String path = "/feeder_status";

    if (!Firebase.RTDB.setString(&fbdo, path + "/feeding_status", data.FB_status)) {
      Serial.println("Failed to set feeding_status: " + fbdo.errorReason());
    }

    if (!Firebase.RTDB.setBool(&fbdo, path + "/isFeeding", data.FB_isFeeding)) {
      Serial.println("Failed to set isFeeding: " + fbdo.errorReason());
    }

    if (!Firebase.RTDB.setDouble(&fbdo, path + "/food_amount", data.FB_foodAmount)) {
      Serial.println("Failed to set food_amount: " + fbdo.errorReason());
    }
  }
}

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
// =========================================================================================================
// ======================================== FIREBASE CONNECTION ============================================
// =========================================================================================================



// =========================================================================================================
// ======================================== HTTP CLOUD FUNCTION ============================================
// =========================================================================================================

enum response {
  DONE,
  FAILED
};

inline response FB_createLog() {
  if (WiFi.status() != WL_CONNECTED) return FAILED;

  HTTPClient http;
  http.begin(CLOUD_FUNCTION_URL);  // use the extern URL
  int code = http.GET();
  http.end();

  return code > 0 ? DONE : FAILED;
}

inline void CL_trigger() {
  int attempts = 0;
  response resp = FB_createLog();
  while (resp == FAILED && attempts < 3) {
    Serial.println("Retrying...");
    delay(2000);
    resp = FB_createLog();
  }
}

// =========================================================================================================
// ======================================== HTTP CLOUD FUNCTION ============================================
// =========================================================================================================



// =========================================================================================================
// ============================================= DATE TIME =================================================
// =========================================================================================================

extern String TIME_now;

inline String getCurrentTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
  }

  char buf[16];
  strftime(buf, sizeof(buf), "%I:%M %p", &timeinfo);

  // eg. 3:45 PM
  return String(buf);  // Convert char[] to Arduino String

  // &timeinfo, "%H:%M:%S" --> 03:45:12
  // &timeinfo, "%d/%m/%Y   %Z" --> 09/11/25 UTC
  // Serial.println(&timeinfo, "%I:%M %p");
}

inline bool TIME_isFeedNow(rtdb_data &sched) {
  if (TIME_now == sched.FB_breakfast) { return true; }
  if (TIME_now == sched.FB_lunch) { return true; }
  if (TIME_now == sched.FB_dinner) { return true; }
  return false;
}

// =========================================================================================================
// ============================================= DATE TIME =================================================
// =========================================================================================================



// =========================================================================================================
// ========================================== MOTOR MOVEMENT ===============================================
// =========================================================================================================

#define L298N_IN1 25
#define L298N_IN2 26

Params_onoff params1 = {
  .pin = L298N_IN1,
  .startState = false,
  .debug = true
};

Params_onoff params2 = {
  .pin = L298N_IN2,
  .startState = false,
  .debug = true
};

ONOFF motorControl_1(params1);
ONOFF motorControl_2(params2);

inline void rotateAction() {
  Serial.println("Rotating motor...");
  motorControl_1.on();
  motorControl_2.off();
}

inline void stopRotateAction() {
  Serial.println("Stopping motor...");
  motorControl_1.off();
  motorControl_2.off();
}

// =========================================================================================================
// ========================================== MOTOR MOVEMENT ===============================================
// =========================================================================================================



// =========================================================================================================
// ======================================== WEIGHT MEASUREMENT =============================================
// =========================================================================================================

struct WEIGHT {
  int FSR_PIN;
  int SAMPLES;              // number of samples for smoothing
  float ADC_noLoad;         // ADC reading (no load)
  float ADC_wLoad;          // ADC reading (with load)
  float WEIGHT_noLoad;      // ADC reading in weight(grams)
  float WEIGHT_wLoad;       // actual reference weight(grams)
  float TARE_offset = 0.0;  // offset if meron pang bowl na buffer sa gitna or something
};

extern WEIGHT WEIGHT_Data;
float slope;

inline void WEIGHT_init() {
  slope = (WEIGHT_Data.WEIGHT_wLoad - WEIGHT_Data.WEIGHT_noLoad) / (WEIGHT_Data.ADC_wLoad - WEIGHT_Data.ADC_noLoad);  // slope
}

inline int WEIGHT_read() {
  long sum = 0;
  for (int i = 0; i < WEIGHT_Data.SAMPLES; ++i) {
    sum += analogRead(WEIGHT_Data.FSR_PIN);
    delay(5);  // buffer
  }
  return sum / WEIGHT_Data.SAMPLES;
}

inline float WEIGHT_getGrams() {
  int raw = WEIGHT_read();
  float grams = slope * (raw - WEIGHT_Data.ADC_noLoad) + WEIGHT_Data.WEIGHT_noLoad;  // linear formula
  grams -= WEIGHT_Data.TARE_offset;                                                  // subtract offset if any
  if (grams < 0.0) grams = 0.0;                                                      // prevent negative values

  Serial.print("ADC: ");
  Serial.print(raw);
  Serial.print("  →  Weight (g): ");
  Serial.println(grams, 1);

  return grams;
}

inline bool WEIGHT_isStopFeeding(rtdb_data &food_amount, float current_weight) {
  double rtdb_weight = food_amount.FB_foodAmount;

  if (rtdb_weight < current_weight) {
    return true;
  } else {
    return false;
  }
}

// =========================================================================================================
// ======================================== WEIGHT MEASUREMENT =============================================
// =========================================================================================================

#endif  // HELPERS_H
