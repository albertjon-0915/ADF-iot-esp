#ifndef ENV_H
#define ENV_H

#ifdef USE_ENV
extern const char* FIREBASE_KEY;
extern const char* FIREBASE_DB_URL;
extern const char* CLOUD_FUNCTION_URL;
const char* AP_SSID;
const char* AP_PSK;
#endif  // END ENV VARIABLE DECLARATION

#endif  // ENV_H