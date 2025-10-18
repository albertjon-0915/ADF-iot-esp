#define USE_ENV
// #define DISABLE_DEBUG

#include "env.h"
#include "Helpers.h"
#include "Utils.h"

#include <WiFi.h>
#include <WebServer.h>

WebServer server(80);

const char* ntpServer = "pool.ntp.org";
const long GMT = 8 * 3600;  // GMT+8 (Philippines time)
const int DST = 0;          // No DST in PH

String NOW_time = "";

void handleRoot() {
  server.send(200, "text/html",
              "<h1>ESP32 WiFi Config</h1>"
              "<p><a href=\"/reset\"><button>Reset WiFi & Start SmartConfig</button></a></p>");
}

void handleReset() {
  prefs.begin("wifi", false);
  prefs.clear();  // delete saved SSID/pass
  prefs.end();

  server.send(200, "text/html", "<p>Credentials cleared. Restarting SmartConfig...</p>");
  delay(1000);
  startSmartConfig();
}

void WIFI_initialize(Wifi& local) {
  if (local.ssid != "") {
    const char* local_ssid = local.ssid.c_str();
    const char* local_pass = local.pass.c_str();

    WiFi.begin(local_ssid, local_pass);
    Serial.printf("Connecting to saved WiFi: %s\n", local_ssid);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
      delay(500);
      Serial.print(".");
    }
  }
}

void assignCurrentTime() {
  NOW_time = getLocalTime();
}

CREATE_ASYNC_FN(GET_dateTime, 15000, assignCurrentTime);

void setup() {
  Serial.begin(115200);
  configTime(GMT, DST, ntpServer);

  WiFi.mode(WIFI_MODE_APSTA);  // AP + STA mode

  // Start AP so user can always connect
  WiFi.softAP(AP_SSID, AP_PSK);
  Serial.print("AP started, connect to SSID: ");
  Serial.println(AP_SSID);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());  // usually 192.168.4.1

  Wifi localWifi = getLocalWifi();  // Load saved WiFi creds
  WIFI_initialize();                // Initialize 1st wifi connection

  if (getWiFiStatus()) {
    Serial.println("\nConnected to STA WiFi!");
  }

  // Setup web server (accessible via AP IP)
  server.on("/", handleRoot);
  server.on("/reset", handleReset);
  server.begin();
  Serial.println("Web server started! Access via AP at http://192.168.4.1/");
}

void loop() {
  server.handleClient();
  asyncDelay(staConnection);


  if (TIME_isFeedNow()) {
    CL_trigger();
    // code here .....
  }
}
