#define L298N_PWM 6 //13
// #define LM393_CPM 34  // will adjust pin
// #define DISABLE_DEBUG

#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#include <Arduino.h>
#include <esp32-hal-ledc.h>  // ensure Arduino LEDC wrappers are available
#include "env.h"
#include "Helpers.h"
#include "Utils.h"
// #include <driver/ledc.h>

// removed WebServer usage
// #include <WebServer.h>

// WebServer server(80);

const char* ntpServer = "pool.ntp.org";
const long GMT = 8 * 3600;  // GMT+8 (Philippines time)
const int DST = 0;          // No DST in PH

// String TIME_now = "";

const int PWM_channel = 0;
const int PWM_freq = 5000;
const int PWM_resolution = 8;

rtdb_data data;

// WEIGHT WEIGHT_Data = {
//   .FSR_PIN = LM393_CPM,
//   .SAMPLES = 20,
//   .ADC_noLoad = 4095,
//   .ADC_wLoad = 1100.0,
//   .WEIGHT_noLoad = 0.0,
//   .WEIGHT_wLoad = 400.0
// };

void assignCurrentTime() {
  TIME_now = getCurrentTime();
  Serial.println(TIME_now);
}

// void assignRtdbData() {
//   data = GET_DATA();
// }

CREATE_ASYNC_FN(GET_dateTime, 15000, assignCurrentTime);
// CREATE_ASYNC_FN(GET_rtdbData, 2000, assignRtdbData);

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Arduino LEDC wrapper (safe with Arduino core)
  // initialize timer/channel with Arduino wrappers
  // ledcSetup(PWM_channel, PWM_freq, PWM_resolution);
  ledcAttachChannel(L298N_PWM, PWM_freq, PWM_resolution, PWM_channel);

  configTime(GMT, DST, ntpServer);
  // WEIGHT_init();

  WiFi.mode(WIFI_MODE_APSTA);  // AP + STA mode (keeps softAP if you want)

  // Start AP so user can always connect (optional)
  WiFi.softAP(AP_SSID, AP_PSK);
  Serial.print("AP started, connect to SSID: ");
  Serial.println(AP_SSID);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());  // usually 192.168.4.1

  // Classic station connect using hardcoded constants.
  // Replace WIFI_SSID and WIFI_PSK with the constants in your env.h if different.
  Serial.print("Connecting to STA WiFi...\n");
  WiFi.begin(STA_SSID, STA_PSK);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(500);
    Serial.print(".");
  }
  // getWiFiStatus();

  firebaseInit();

  // removed WebServer setup
  // server.on("/", handleRoot);
  // server.on("/reset", handleReset);
  // server.begin();
  // Serial.println("Web server started! Access via AP at http://192.168.4.1/");
}

void loop() {
  // removed server.handleClient();
  asyncDelay(GET_dateTime);
  // asyncDelay(GET_rtdbData);

  // set duty using Arduino wrapper (0 .. 2^PWM_resolution - 1)
  ledcWrite(PWM_channel, 155);
  // float weight = WEIGHT_getGrams();  // read analog value and convert to grams
  // uint32_t duty = 155; // 0..2^PWM_resolution-1
  // ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
  // ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

  // if (TIME_isFeedNow(data)) {

  //   SEND_DATA(DISPENSING);
  //   rotateAction();

  //   if (WEIGHT_isStopFeeding(data, weight)) {
  //     SEND_DATA(FOODREADY);
  //     stopRotateAction();
  //     CL_trigger();
  //     return;
  //   }
  // }

  // // if the food is already eaten by the pet, update status
  // if (weight < 100) {
  //   SEND_DATA(IDLE);
  // }

  // app.loop();
  // // Check if authentication is ready
  // if (app.ready()) {}
  firebasePoll(); 
}
