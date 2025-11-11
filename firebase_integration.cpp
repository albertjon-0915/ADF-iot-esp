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
RealtimeDatabase Database;  // Ensure RealtimeDatabase is properly declared

enum UIDCode {
  U_RTB_STATUS,
  U_RTB_FOOD,
  U_RTB_ISFEEDING,
  U_RTB_BREAKFAST,
  U_RTB_LUNCH,
  U_RTB_DINNER,
  U_UNKNOWN
};

static UIDCode uidToCode(const String &uid) {
  if (uid == "rtb_status") return U_RTB_STATUS;
  if (uid == "rtb_food") return U_RTB_FOOD;
  if (uid == "rtb_isFeeding") return U_RTB_ISFEEDING;
  if (uid == "rtb_breakfast") return U_RTB_BREAKFAST;
  if (uid == "rtb_lunch") return U_RTB_LUNCH;
  if (uid == "rtb_dinner") return U_RTB_DINNER;
  return U_UNKNOWN;
}

// Local callback that fills the shared globals declared in Helpers.cpp
void processData(AsyncResult &aResult) {
  if (!aResult.isResult()) return;
  Serial.println("processing data fetched");

  if (aResult.isError()) {
    Serial.printf("Firebase error: %s (uid=%s)\n", aResult.error().message().c_str(), aResult.uid().c_str());
    return;
  }
  if (!aResult.available()) return;

  String uid = aResult.uid();
  String payload = aResult.c_str();
  Serial.printf("%s : %s\n", uid.c_str(), payload.c_str());
  // Strip surrounding quotes for strings
  if (payload.length() >= 2 && payload.startsWith("\"") && payload.endsWith("\"")) {
    payload = payload.substring(1, payload.length() - 1);
  }

  switch (uidToCode(uid)) {
    case U_RTB_STATUS:
      jsonResp.FB_status = payload;
      Serial.println("FB status -> " + jsonResp.FB_status);
      break;
    case U_RTB_FOOD:
      jsonResp.FB_foodAmount = payload.toFloat();
      Serial.printf("FB foodAmount -> %.2f\n", jsonResp.FB_foodAmount);
      break;
    case U_RTB_ISFEEDING:
      jsonResp.FB_isFeeding = (payload == "true" || payload == "1");
      Serial.printf("FB isFeeding -> %d\n", jsonResp.FB_isFeeding);
      break;
    case U_RTB_BREAKFAST:
      jsonResp.FB_breakfast = payload;
      Serial.println("FB breakfast -> " + jsonResp.FB_breakfast);
      break;
    case U_RTB_LUNCH:
      jsonResp.FB_lunch = payload;
      Serial.println("FB lunch -> " + jsonResp.FB_lunch);
      break;
    case U_RTB_DINNER:
      jsonResp.FB_dinner = payload;
      Serial.println("FB dinner -> " + jsonResp.FB_dinner);
      break;
    default:
      Serial.printf("Unhandled UID: %s payload: %s\n", uid.c_str(), payload.c_str());
      break;
  }
}

void firebaseInit() {
  ssl_client.setInsecure();
  ssl_client.setConnectionTimeout(3000);
  ssl_client.setHandshakeTimeout(5);

  initializeApp(aClient, app, getAuth(user_auth), processData, "authTask");
  app.getApp<RealtimeDatabase>(Database);  // Corrected initialization
  Database.url(FIREBASE_DB_URL);
  Serial.println("FirebaseClient initialization requested");
}

static unsigned long lastPoll = 0;
const unsigned long POLL_MS = 2000;

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
}

void firebaseSendStatus(const rtdb_data &d) {
  if (!app.ready()) return;
  // Async set calls (no callback provided here)
  Database.set<String>(aClient, "/feeder_status/feeding_status", d.FB_status, nullptr, "US");
  Database.set<bool>(aClient, "/feeder_status/isFeeding", d.FB_isFeeding, nullptr, "UI");
}