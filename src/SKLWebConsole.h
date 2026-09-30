// SKLWebConsole -- a browser-based serial console for ESP32 projects.
// Copyright (c) 2026 SKL (Martin Abbott). See LICENSE.
//
// Anything you print() / println() / printf() to WebConsole shows up live in
// a browser at http://<device-ip>/webserial (path configurable), and lines
// typed into the page come back to your onMessage() handler.
//
//   #include <SKLWebConsole.h>
//   AsyncWebServer server(80);
//
//   void setup() {
//     WebConsole.setTitle("Kitchen sensor");
//     WebConsole.addQuickCommand("Status", "status");
//     WebConsole.onMessage([](const String& cmd) { WebConsole.println("You said: " + cmd); });
//     WebConsole.begin(&server);          // page at /webserial
//     server.begin();
//   }
//   void loop() { WebConsole.loop(); }
//
// What it does beyond a bare WebSocket log:
//   - Keeps the last few KB of output on the device and replays it to each
//     browser that connects, so you see boot messages and anything printed
//     while no page was open.
//   - Numbers every line, so a reconnecting page never shows duplicates and
//     can tell you when lines were skipped.
//   - Safe to print from any task (loop, web handlers, timers): output is
//     assembled into whole lines under a mutex.
//   - Optional access control: HTTP Basic credentials, or your own check
//     (for example "has a valid admin session cookie").
//   - Page is served gzipped from flash; no internet or CDN needed.
//
// Requires ESP32 + ESPAsyncWebServer 3.x (ESP32Async) + AsyncTCP.

#pragma once

#include <Arduino.h>
#include <functional>
#include <vector>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#define SKL_WEBCONSOLE_VERSION "1.1.1"

// Longest single line kept; longer output is split into several lines.
#ifndef SKL_WEBCONSOLE_LINE_MAX
#define SKL_WEBCONSOLE_LINE_MAX 512
#endif

// Default size of the on-device history replayed to new browsers (bytes).
#ifndef SKL_WEBCONSOLE_HISTORY_BYTES
#define SKL_WEBCONSOLE_HISTORY_BYTES 8192
#endif

class SKLWebConsole : public Print {
 public:
  using MessageHandler = std::function<void(uint8_t* data, size_t len)>;
  using StringMessageHandler = std::function<void(const String& message)>;
  using AuthCheck = std::function<bool(AsyncWebServerRequest* request)>;
  using TitleProvider = std::function<String()>;

  SKLWebConsole() = default;

  // Registers the page at `path`, the WebSocket at `path`/ws and an access
  // check at `path`/auth. Call before server->begin(). The console captures
  // output from the very first print, even before begin() runs.
  void begin(AsyncWebServer* server, const char* path = "/webserial");

  // Called (on the web server's task) with each line typed into the page.
  void onMessage(MessageHandler handler);
  void onMessage(StringMessageHandler handler);

  // Access control. Use either or both; with neither, anyone who can reach
  // the device can open the console.
  //  - setAuthentication: HTTP Basic user/password (browser prompts).
  //  - setAuthCheck: your own test. When it fails, the page shows a
  //    "Sign in required" card, linking to loginUrl if you give one.
  void setAuthentication(const char* username, const char* password);
  void setAuthCheck(AuthCheck check, const char* loginUrl = nullptr);

  // Page header. setTitleProvider is re-evaluated on every connection
  // (handy when the device name can change at runtime).
  void setTitle(const String& title);
  void setTitleProvider(TitleProvider provider);

  // One-click buttons under the command box.
  void addQuickCommand(const String& label, const String& command);
  void clearQuickCommands();

  // History buffer size in bytes (0 disables replay). Takes effect only if
  // called before the first print.
  void setHistorySize(size_t bytes);

  // Max simultaneous browser connections (oldest dropped). Default 3.
  void setMaxClients(uint8_t n) { _maxClients = n ? n : 1; }

  // Housekeeping (drops dead connections). Call from loop(); cheap.
  void loop();

  size_t clientCount() const;
  uint32_t droppedLines() const { return _dropped; }

  // Print interface
  size_t write(uint8_t c) override;
  size_t write(const uint8_t* buffer, size_t size) override;
  using Print::write;

  // Text and line ending go out as one unit, so a line printed from one task
  // can't be spliced into a line printed from another at the same moment.
  using Print::println;
  size_t println(const char* s);
  size_t println(const String& s);
  size_t println(const __FlashStringHelper* s);

 private:
  void _ensureInit();
  void _lock();
  void _unlock();
  void _endLine(String& out);          // under lock: finish _line, append "seq\ttext\n" to out
  void _histAppend(const char* s, size_t n);
  void _histDropOldest();
  void _sendLive(const String& out);
  bool _authorized(AsyncWebServerRequest* request);
  void _onEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len);
  String _helloJson();

  AsyncWebServer* _server = nullptr;
  AsyncWebSocket* _ws = nullptr;
  String _path;
  String _title;
  TitleProvider _titleProvider;
  MessageHandler _handler;
  StringMessageHandler _stringHandler;
  AuthCheck _authCheck;
  String _loginUrl;
  String _user, _pass;
  std::vector<std::pair<String, String>> _quick;
  uint8_t _maxClients = 3;
  uint32_t _bootId = 0;
  unsigned long _lastCleanup = 0;

  SemaphoreHandle_t _mutex = nullptr;

  char _line[SKL_WEBCONSOLE_LINE_MAX];
  size_t _lineLen = 0;
  uint32_t _seq = 0;                   // number of complete lines so far
  uint32_t _dropped = 0;

  char* _hist = nullptr;               // circular buffer of '\n'-terminated lines
  size_t _histCap = SKL_WEBCONSOLE_HISTORY_BYTES;
  size_t _histStart = 0, _histUsed = 0;
  uint32_t _histLines = 0;
  bool _histAllocTried = false;
};

extern SKLWebConsole WebConsole;
