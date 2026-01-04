#include "Helpers.h"
#include <FirebaseClient.h>
#include <WiFiClientSecure.h>

rtdb_data jsonResp = {
  .FB_breakfast = "",
  .FB_lunch = "",
  .FB_dinner = "",
  .FB_status = "IDLE",
  .FB_foodAmount = 0.0,
  .FB_isFeeding = false,
};

// FirebaseClient objects and anonymous auth (empty email/pass)
UserAuth user_auth(FIREBASE_KEY, AUTH_EMAIL, AUTH_PASS);  // anonymous sign-in
FirebaseApp app;
WiFiClientSecure ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);
RealtimeDatabase Database;

enum UIDCode {
  U_RTB_STATUS,
  U_RTB_FOOD,
  U_RTB_ISFEEDING,
  U_RTB_BREAKFAST,
  U_RTB_LUNCH,
  U_RTB_DINNER,
  // REVISED
  // U_RTB_1ST,
  // U_RTB_2ND,
  // U_RTB_3RD,
  // U_RTB_4TH,
  // U_RTB_5TH,
  U_UNKNOWN
};

static UIDCode uidToCode(const String &uid) {
  if (uid == "rtb_status") return U_RTB_STATUS;
  if (uid == "rtb_food") return U_RTB_FOOD;
  if (uid == "rtb_isFeeding") return U_RTB_ISFEEDING;
  if (uid == "rtb_breakfast") return U_RTB_BREAKFAST;
  if (uid == "rtb_lunch") return U_RTB_LUNCH;
  if (uid == "rtb_dinner") return U_RTB_DINNER;

  // REVISED
  // if (uid == "rtb_1ST") return U_RTB_1ST;
  // if (uid == "rtb_2ND") return U_RTB_2ND;
  // if (uid == "rtb_3RD") return U_RTB_3RD;
  // if (uid == "rtb_4TH") return U_RTB_4TH;
  // if (uid == "rtb_5TH") return U_RTB_5TH;
  return U_UNKNOWN;
}

// Callback
void processData(AsyncResult &aResult) {
  if (!aResult.isResult()) return;

  if (aResult.isError()) {
    Serial.printf("Firebase error: %s (uid=%s)\n", aResult.error().message().c_str(), aResult.uid().c_str());
    return;
  }
  if (!aResult.available()) {
    Serial.println("No handshake with the RTDB, waiting for data...");
    return;
  }

  String uid = aResult.uid();
  String payload = aResult.c_str();
  // Strip surrounding quotes for strings
  if (payload.length() >= 2 && payload.startsWith("\"") && payload.endsWith("\"")) {
    payload = payload.substring(1, payload.length() - 1);
  }

  // NOTE: **ADD PROCESS FOR REVISED FEEDER SCHEDULE
  // U_RTB_1ST,
  // U_RTB_2ND,
  // U_RTB_3RD,
  // U_RTB_4TH,
  // U_RTB_5TH,
  switch (uidToCode(uid)) {
    case U_RTB_STATUS:
      jsonResp.FB_status = payload;
      Serial.println("FB status -> " + jsonResp.FB_status);
      break;
    case U_RTB_FOOD:
      jsonResp.FB_foodAmount = payload.toFloat();
      // Serial.printf("FB foodAmount -> %.2f\n", jsonResp.FB_foodAmount);
      break;
    case U_RTB_ISFEEDING:
      jsonResp.FB_isFeeding = (payload == "true" || payload == "1");
      // Serial.printf("FB isFeeding -> %d\n", jsonResp.FB_isFeeding);
      break;
    case U_RTB_BREAKFAST:
      jsonResp.FB_breakfast = payload;
      // Serial.println("FB breakfast -> " + jsonResp.FB_breakfast);
      break;
    case U_RTB_LUNCH:
      jsonResp.FB_lunch = payload;
      // Serial.println("FB lunch -> " + jsonResp.FB_lunch);
      break;
    case U_RTB_DINNER:
      jsonResp.FB_dinner = payload;
      // Serial.println("FB dinner -> " + jsonResp.FB_dinner);
      break;
    default:
      Serial.printf("Unhandled UID: %s payload: %s\n", uid.c_str(), payload.c_str());
      break;
  }
}

void checkHandShake(AsyncResult &aResult) {
  if (!aResult.isResult())
    return;

  if (aResult.isEvent())
    Firebase.printf("Event task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.eventLog().message().c_str(), aResult.eventLog().code());

  if (aResult.isDebug())
    Firebase.printf("Debug task: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());

  if (aResult.isError())
    Firebase.printf("Error task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.error().message().c_str(), aResult.error().code());

  if (aResult.available()) 
    Firebase.printf("task: %s, payload: %s\n", aResult.uid().c_str(), aResult.c_str());
}

void firebaseInit() {
  ssl_client.setInsecure();
  ssl_client.setConnectionTimeout(3000);
  ssl_client.setHandshakeTimeout(10000);

  initializeApp(aClient, app, getAuth(user_auth), checkHandShake, "AUTH TASK: ");
  app.getApp<RealtimeDatabase>(Database);
  Database.url(FIREBASE_DB_URL);
  Serial.println("FirebaseClient initialization requested");
}

static unsigned long lastPoll = 0;
const unsigned long POLL_MS = 6000;  // asynchronous timer

// Timed polling
void firebasePoll() {
  app.loop();
  if (!app.ready()) return;

  unsigned long now = millis();
  if (now - lastPoll < POLL_MS) return;
  lastPoll = now;

  // Async gets — callback will update globals
  Database.get(aClient, "/feeder_status/feeding_status", processData, false, "rtb_status");
  Database.get(aClient, "/feeder_status/food_amount", processData, false, "rtb_food");
  Database.get(aClient, "/feeder_status/isFeeding", processData, false, "rtb_isFeeding");

  Database.get(aClient, "/feeder_status/breakfast_sched", processData, false, "rtb_breakfast");
  Database.get(aClient, "/feeder_status/lunch_sched", processData, false, "rtb_lunch");
  Database.get(aClient, "/feeder_status/dinner_sched", processData, false, "rtb_dinner");

  // REVISED
  // Database.get(aClient, "/feeder_status/schedules/1st", processData, false, "rtb_1st");
  // Database.get(aClient, "/feeder_status/schedules/2nd", processData, false, "rtb_2nd");
  // Database.get(aClient, "/feeder_status/schedules/3rd", processData, false, "rtb_3rd");
  // Database.get(aClient, "/feeder_status/schedules/4th", processData, false, "rtb_4th");
  // Database.get(aClient, "/feeder_status/schedules/5th", processData, false, "rtb_5th");
}

void firebaseSendStatus(const rtdb_data &d) {
  app.loop();
  if (!app.ready()) return;

  bool assignValues = true;
  Serial.print(d.FB_status);
  Serial.print(" : ");
  Serial.print(d.FB_isFeeding);
  Serial.print(" -> ");
  Serial.println("sending data to rtdb");

  // Async set calls (no callback provided here) -> nullptr
  // Database.set<String>(aClient, "/feeder_status/feeding_status", d.FB_status, nullptr, "US");
  // Database.set<bool>(aClient, "/feeder_status/isFeeding", d.FB_isFeeding, nullptr, "UI");

  // Switch to synchronous set calls
  bool okStatus = Database.set<string_t>(aClient, "/feeder_status/feeding_status", string_t(d.FB_status));
  if (!okStatus) {
    Serial.print("RTDB -> feeding_status update error... ");
    assignValues = false;
  } else {
    Serial.println("RTDB -> feeding_status updated!");
  }

  bool okFeeding = Database.set<boolean_t>(aClient, "/feeder_status/isFeeding", boolean_t(d.FB_isFeeding));
  if (!okFeeding) {
    Serial.print("RTDB -> isFeeding update error... ");
    assignValues = false;
  } else {
    Serial.println("RTDB -> isFeeding updated!");
  }

  // Get and assign freshly update values to jsonResp
  if (assignValues) {

    String jsonRespStatus = Database.get<String>(aClient, "/feeder_status/feeding_status");
    bool jsonRespFeeding = Database.get<bool>(aClient, "/feeder_status/isFeeding");

    jsonResp.FB_status = jsonRespStatus;
    jsonResp.FB_isFeeding = jsonRespFeeding;
  }
}

void UPDATE(STAGE stage) {
  rtdb_data *d;  // this is a pointer
  // use & (if you rebind later)
  switch (stage) {
    case FIRST: d = &DISPENSING; break;
    case SECOND: d = &FOODREADY; break;
    case FINAL: d = &IDLE; break;
    default: d = &IDLE; break;
  }

  firebaseSendStatus(*d);
}