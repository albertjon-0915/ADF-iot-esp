#ifndef HELPERS_H
#define HELPERS_H

#include <Preferences.h>
#include <ONOFF.h> // include library on sketch (https://github.com/albertjon-0915/ON_OFF)
#include <HTTPClient.h>
#include <Firebase_ESP_Client.h>
#include <FirebaseJson.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include "time.h"
#include "env.h"

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
  if (!wifi.pass.isEmpty()) { prefs.putString("pass", wifi.psk); }
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

  // Save new credentials
  wifi = { WiFi.SSID, WiFi.psk };
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
    return false;
  }
}

// =========================================================================================================
// ========================================== WIFI CONNECTION ==============================================
// =========================================================================================================


// =========================================================================================================
// ======================================== FIREBASE CONNECTION ============================================
// =========================================================================================================

enum CON_STATUS {
  INVALID_CREDS,
  ERROR,
  OK
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
    return OK;
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
  rtdb_data jsonResp;

  if (Firebase.ready() && fireBaseConnect() == OK) {
    if (Firebase.RTDB.getJSON(&fbdo, "feeder_status")) {
      FirebaseJson &json = fbdo.jsonObject();

      json.get(jsonResp.FB_breakfast, "breakfast_sched");
      json.get(jsonResp.FB_lunch, "lunch_sched");
      json.get(jsonResp.FB_dinner, "dinner_sched");
      json.get(jsonResp.FB_status, "feeding_status");
      json.get(jsonResp.FB_foodAmount, "food_amount");
      json.get(jsonResp.FB_isFeeding, "isFeeding");
    }
  }

  return jsonResp;
}

inline void SEND_DATA(rtdb_data &data) {
  if (Firebase.ready() && fireBaseConnect() == OK) {
    FirebaseJson json;
    json.set("feeding_status", data.FB_status);
    json.set("isFeeding", data.FB_isFeeding);

    /*
      Can use(set/get):
      - set
      - setInt
      - setFloat
      - setDouble
      - setString
      - setJSON
      - setArray
      - setBlob
      - setFile
      - updateNode --> for advanced use/to update individual node without affecting other json files
    */
    if (Firebase.RTDB.updateNode(&fbdo, "feeder_status", &json)) {
      Serial.println(getLocalTime());
      Serial.println("Successfully saved at" + fbdo.dataPath());
      Serial.println("( " + fbdo.dataPath() + " )");
    } else {
      Serial.println("Failed: " + fbdo.errorReason());
    }
  }
}

// =========================================================================================================
// ======================================== FIREBASE CONNECTION ============================================
// =========================================================================================================


// =========================================================================================================
// ======================================== HTTP CLOUD FUNCTION ============================================
// =========================================================================================================

enum response {
  SUCCESS,
  FAILED
};

inline response FB_createLog() {
  if (WiFi.status() != WL_CONNECTED) return FAILED;

  HTTPClient http;
  http.begin(CLOUD_FUNCTION_URL);  // use the extern URL
  int code = http.GET();
  http.end();

  return code > 0 ? SUCCESS : FAILED;
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

extern String NOW_time;


inline String getLocalTime() {
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
  if (NOW_time == sched.FB_breakfast) { return true; }
  if (NOW_time == sched.FB_lunch) { return true; }
  if (NOW_time == sched.FB_dinner) { return true; }
  return false;
}

// =========================================================================================================
// ============================================= DATE TIME =================================================
// =========================================================================================================


// =========================================================================================================
// ========================================== MOTOR MOVEMENT ===============================================
// =========================================================================================================

#define L298N_IN1 11
#define L298N_IN2 10

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

void rotateAction() {  // adjust to to know whether to extend or retract
  motorControl_1.on();
  motorControl_2.off();
}

void stopRotateAction(){
  motorControl_1.off();
  motorControl_2.off();
}

// =========================================================================================================
// ========================================== MOTOR MOVEMENT ===============================================
// =========================================================================================================

#endif  // HELPERS_H