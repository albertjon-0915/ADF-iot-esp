#ifndef UTILS_H
#define UTILS_H

#include <type_traits>

// Disable Serial.print/println
// #define DISABLE_DEBUG --> usage(declare on .ino file)
#ifndef DISABLE_DEBUG
// Normal Serial (Arduino built-in)
#else
struct DebugClass : public Stream {
  void begin(long) {}
  void end() {}
  void flush() {}
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  size_t write(uint8_t) override { return 1; }

  template <typename T>
  void print(const T&) {}
  template <typename T>
  void println(const T&) {}
  void println() {}
  void printf(const char*, ...) {}
} debugSerial;
#define Serial debugSerial
#endif

// TTimer struct
template<typename Func>
struct TTimer {
  unsigned long lastRun = 0;
  unsigned long interval;
  Func callback;
};

// Helper macro
#define CREATE_ASYNC_FN(name, interval, func) TTimer<decltype(func)*> name{ 0, interval, func };
#define CREATE_ASYNC_OBJ(name, interval, func) TTimer<decltype(func)> name{ 0, interval, func };

// void version
template<typename Func>
inline void asyncDelay(TTimer<Func>& t, std::enable_if_t<std::is_same_v<decltype(t.callback()), void>, int> = 0) {
  unsigned long now = millis();
  if (now - t.lastRun >= t.interval) {
    t.lastRun = now;
    t.callback();
  }
}

// non-void version
template<typename Func>
inline auto asyncDelay(TTimer<Func>& t, std::enable_if_t<!std::is_same_v<decltype(t.callback()), void>, int> = 0) {
  unsigned long now = millis();
  if (now - t.lastRun >= t.interval) {
    t.lastRun = now;
    return t.callback();  // directly return the value
  }
  return decltype(t.callback()){};  // default value if not executed
}

#endif  // UTILS_H
