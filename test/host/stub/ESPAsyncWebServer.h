#pragma once
// Host mock mirroring the ESPAsyncWebServer 3.6.0 signatures SKLWebConsole uses.
#include "Arduino.h"
#include <functional>
#include <vector>
#include <map>
#include <mutex>
typedef enum { WS_CONTINUATION, WS_TEXT, WS_BINARY } AwsFrameType;
typedef enum { WS_EVT_CONNECT, WS_EVT_DISCONNECT, WS_EVT_PING, WS_EVT_PONG, WS_EVT_ERROR, WS_EVT_DATA } AwsEventType;
typedef struct { uint8_t message_opcode; uint32_t num; uint8_t final; uint8_t masked; uint8_t opcode; uint64_t len; uint8_t mask[4]; uint64_t index; } AwsFrameInfo;
enum WebRequestMethod { HTTP_GET = 1 };
class AsyncWebServerResponse {
 public:
  int code; std::string type, body; std::map<std::string, std::string> headers;
  bool addHeader(const char* n, const char* v, bool = true) { headers[n] = v; return true; }
};
class AsyncWebServerRequest {
 public:
  bool basicOk = true; bool cookieOk = true; bool askedAuth = false; AsyncWebServerResponse* sent = nullptr;
  bool authenticate(const char*, const char*, const char* = nullptr, bool = false) const { return basicOk; }
  void requestAuthentication(const char* = nullptr, bool = true) { askedAuth = true; }
  AsyncWebServerResponse* beginResponse(int c, const char* t, const String& b) { auto r = new AsyncWebServerResponse(); r->code = c; r->type = t; r->body = b.s; return r; }
  AsyncWebServerResponse* beginResponse(int c, const char* t, const uint8_t* d, size_t n) { auto r = new AsyncWebServerResponse(); r->code = c; r->type = t; r->body.assign((const char*)d, n); return r; }
  void send(AsyncWebServerResponse* r) { sent = r; }
};
using ArRequestHandlerFunction = std::function<void(AsyncWebServerRequest*)>;
class AsyncWebHandler { public: virtual ~AsyncWebHandler() {} };
class AsyncWebSocket;
class AsyncWebSocketClient {
 public:
  std::vector<std::string> frames;
  bool text(const String& m) { frames.push_back(m.s); return true; }
};
using AwsEventHandler = std::function<void(AsyncWebSocket*, AsyncWebSocketClient*, AwsEventType, void*, uint8_t*, size_t)>;
using AwsHandshakeHandler = std::function<bool(AsyncWebServerRequest*)>;
class AsyncWebSocket : public AsyncWebHandler {
 public:
  std::string url; AwsEventHandler ev; AwsHandshakeHandler hs;
  std::vector<AsyncWebSocketClient*> clients; std::vector<std::string> all; bool full = false; int cleanups = 0;
  std::mutex m;
  AsyncWebSocket(const String& u) : url(u.s) {}
  void onEvent(AwsEventHandler h) { ev = h; }
  void handleHandshake(AwsHandshakeHandler h) { hs = h; }
  size_t count() const { return clients.size(); }
  bool availableForWriteAll() { return !full; }
  int textAll(const String& s) { std::lock_guard<std::mutex> g(m); all.push_back(s.s); return 0; }
  void cleanupClients(uint16_t) { cleanups++; }
};
class AsyncWebServer {
 public:
  std::vector<AsyncWebHandler*> handlers; std::vector<std::pair<std::string, ArRequestHandlerFunction>> routes;
  void addHandler(AsyncWebHandler* h) { handlers.push_back(h); }
  void on(const char* uri, int, ArRequestHandlerFunction f) { routes.push_back({uri, f}); }
};
