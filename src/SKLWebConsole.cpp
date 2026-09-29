// SKLWebConsole -- see SKLWebConsole.h for usage.
// Copyright (c) 2026 SKL (Martin Abbott). See LICENSE.

#include "SKLWebConsole.h"
#include "SKLWebConsolePage.h"
#include <esp_system.h>

SKLWebConsole WebConsole;

static portMUX_TYPE s_initMux = portMUX_INITIALIZER_UNLOCKED;

static void jsonEscapeInto(String& out, const String& s) {
  out += '"';
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    switch (c) {
      case '"':  out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if ((uint8_t)c < 0x20) {
          char buf[7];
          snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)(uint8_t)c);
          out += buf;
        } else {
          out += c;
        }
    }
  }
  out += '"';
}

// --------------------------------------------------------------------------
// Setup
// --------------------------------------------------------------------------

void SKLWebConsole::begin(AsyncWebServer* server, const char* path) {
  _ensureInit();
  if (!server || _ws) return;
  _server = server;
  _path = (path && *path) ? path : "/webserial";
  if (_path.length() > 1 && _path.endsWith("/")) _path.remove(_path.length() - 1);
  const String base = (_path == "/") ? String("") : _path;
  if (!_bootId) _bootId = esp_random();

  // Order matters: ESPAsyncWebServer also matches "<path>/..." against a
  // handler registered for "<path>", so the more specific ones go first.
  _ws = new AsyncWebSocket(base + "/ws");
  _ws->handleHandshake([this](AsyncWebServerRequest* request) { return _authorized(request); });
  _ws->onEvent([this](AsyncWebSocket* s, AsyncWebSocketClient* c, AwsEventType t, void* arg, uint8_t* data, size_t len) {
    _onEvent(s, c, t, arg, data, len);
  });
  server->addHandler(_ws);

  server->on((base + "/auth").c_str(), HTTP_GET, [this](AsyncWebServerRequest* request) {
    String body = "{\"authed\":";
    body += _authorized(request) ? "true" : "false";
    if (_loginUrl.length()) {
      body += ",\"login\":";
      jsonEscapeInto(body, _loginUrl);
    }
    body += '}';
    AsyncWebServerResponse* response = request->beginResponse(200, "application/json", body);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
  });

  server->on(_path.c_str(), HTTP_GET, [this](AsyncWebServerRequest* request) {
    // Basic auth gates the page itself so the browser prompts once; a custom
    // AuthCheck is enforced on the WebSocket, and the page explains it.
    if (_user.length() && !request->authenticate(_user.c_str(), _pass.c_str())) {
      request->requestAuthentication("SKLWebConsole", false);
      return;
    }
    AsyncWebServerResponse* response =
        request->beginResponse(200, "text/html", SKL_WEBCONSOLE_PAGE_GZ, SKL_WEBCONSOLE_PAGE_GZ_LEN);
    response->addHeader("Content-Encoding", "gzip");
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
  });
}

void SKLWebConsole::onMessage(MessageHandler handler) { _handler = handler; }
void SKLWebConsole::onMessage(StringMessageHandler handler) { _stringHandler = handler; }

void SKLWebConsole::setAuthentication(const char* username, const char* password) {
  _user = username ? username : "";
  _pass = password ? password : "";
}

void SKLWebConsole::setAuthCheck(AuthCheck check, const char* loginUrl) {
  _authCheck = check;
  _loginUrl = loginUrl ? loginUrl : "";
}

void SKLWebConsole::setTitle(const String& title) { _title = title; }
void SKLWebConsole::setTitleProvider(TitleProvider provider) { _titleProvider = provider; }

void SKLWebConsole::addQuickCommand(const String& label, const String& command) {
  _quick.emplace_back(label, command);
}
void SKLWebConsole::clearQuickCommands() { _quick.clear(); }

void SKLWebConsole::setHistorySize(size_t bytes) {
  if (!_histAllocTried) _histCap = bytes;
}

void SKLWebConsole::loop() {
  if (!_ws) return;
  unsigned long now = millis();
  if (now - _lastCleanup >= 2000) {
    _lastCleanup = now;
    _ws->cleanupClients(_maxClients);
  }
}

size_t SKLWebConsole::clientCount() const { return _ws ? _ws->count() : 0; }

// --------------------------------------------------------------------------
// Locking / buffers
// --------------------------------------------------------------------------

void SKLWebConsole::_ensureInit() {
  if (_mutex) return;
  SemaphoreHandle_t m = xSemaphoreCreateRecursiveMutex();
  bool mine = false;
  portENTER_CRITICAL(&s_initMux);
  if (!_mutex) {
    _mutex = m;
    mine = true;
  }
  portEXIT_CRITICAL(&s_initMux);
  if (!mine && m) vSemaphoreDelete(m);
}

void SKLWebConsole::_lock() {
  if (_mutex) xSemaphoreTakeRecursive(_mutex, portMAX_DELAY);
}

void SKLWebConsole::_unlock() {
  if (_mutex) xSemaphoreGiveRecursive(_mutex);
}

void SKLWebConsole::_histDropOldest() {
  if (!_histUsed) return;
  size_t n = 0;
  while (n < _histUsed) {
    char c = _hist[(_histStart + n) % _histCap];
    n++;
    if (c == '\n') break;
  }
  _histStart = (_histStart + n) % _histCap;
  _histUsed -= n;
  if (_histLines) _histLines--;
}

void SKLWebConsole::_histAppend(const char* s, size_t n) {
  if (!_hist || _histCap < 2) return;
  if (n + 1 > _histCap) n = _histCap - 1;
  while (_histUsed + n + 1 > _histCap) _histDropOldest();
  size_t pos = (_histStart + _histUsed) % _histCap;
  for (size_t i = 0; i < n; i++) {
    _hist[pos] = s[i];
    pos = (pos + 1) % _histCap;
  }
  _hist[pos] = '\n';
  _histUsed += n + 1;
  _histLines++;
}

void SKLWebConsole::_endLine(String& out) {
  _line[_lineLen] = '\0';
  const uint32_t seq = _seq++;
  _histAppend(_line, _lineLen);
  if (_ws && _ws->count()) {
    out += String(seq);
    out += '\t';
    out += _line;
    out += '\n';
  }
  _lineLen = 0;
}

// --------------------------------------------------------------------------
// Print
// --------------------------------------------------------------------------

size_t SKLWebConsole::write(uint8_t c) { return write(&c, 1); }

size_t SKLWebConsole::println(const char* s) {
  _ensureInit();
  _lock();  // recursive: write() takes it again
  size_t n = print(s);
  n += Print::println();
  _unlock();
  return n;
}

size_t SKLWebConsole::println(const String& s) { return println(s.c_str()); }

size_t SKLWebConsole::println(const __FlashStringHelper* s) { return println(reinterpret_cast<const char*>(s)); }

size_t SKLWebConsole::write(const uint8_t* buffer, size_t size) {
  if (!buffer || !size) return 0;
  _ensureInit();
  String out;
  _lock();
  if (!_histAllocTried) {
    _histAllocTried = true;
    if (_histCap) _hist = (char*)malloc(_histCap);
    if (!_hist) _histCap = 0;
  }
  for (size_t i = 0; i < size; i++) {
    const char c = (char)buffer[i];
    if (c == '\r') continue;
    if (c == '\n') {
      _endLine(out);
      continue;
    }
    if (c == '\0') continue;  // keeps lines printable and NUL-terminated
    if (_lineLen >= SKL_WEBCONSOLE_LINE_MAX - 1) _endLine(out);
    _line[_lineLen++] = c;
  }
  _unlock();
  if (out.length()) _sendLive(out);
  return size;
}

void SKLWebConsole::_sendLive(const String& out) {
  if (!_ws) return;
  if (!_ws->availableForWriteAll()) {
    // A browser's queue is full (slow link). Skip rather than block the
    // caller; the page shows "N lines skipped" from the sequence gap, and
    // the lines are still in the on-device history for the next connect.
    for (size_t i = 0; i < out.length(); i++)
      if (out[i] == '\n') _dropped++;
    return;
  }
  _ws->textAll(out);
}

// --------------------------------------------------------------------------
// WebSocket
// --------------------------------------------------------------------------

bool SKLWebConsole::_authorized(AsyncWebServerRequest* request) {
  if (_user.length() && !request->authenticate(_user.c_str(), _pass.c_str())) return false;
  if (_authCheck && !_authCheck(request)) return false;
  return true;
}

String SKLWebConsole::_helloJson() {
  String title = _titleProvider ? _titleProvider() : _title;
  String j = "\x01{\"title\":";
  jsonEscapeInto(j, title);
  j += ",\"version\":\"" SKL_WEBCONSOLE_VERSION "\"";
  j += ",\"boot\":";
  j += String(_bootId);
  _lock();
  j += ",\"hist\":";
  j += String(_histLines);
  _unlock();
  j += ",\"commands\":[";
  for (size_t i = 0; i < _quick.size(); i++) {
    if (i) j += ',';
    j += "{\"l\":";
    jsonEscapeInto(j, _quick[i].first);
    j += ",\"c\":";
    jsonEscapeInto(j, _quick[i].second);
    j += '}';
  }
  j += "]}";
  return j;
}

void SKLWebConsole::_onEvent(AsyncWebSocket*, AsyncWebSocketClient* client, AwsEventType type, void* arg,
                             uint8_t* data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    client->text(_helloJson());
    // Replay the history as one frame: "\x02seq\ttext\n...". Sent even when
    // empty so the page knows the replay is done and can show live lines.
    String replay;
    _lock();
    replay.reserve(_histUsed + _histLines * 8 + 2);
    replay += '\x02';
    uint32_t seq = _seq - _histLines;
    bool lineStart = true;
    for (size_t i = 0; i < _histUsed; i++) {
      if (lineStart) {
        replay += String(seq++);
        replay += '\t';
        lineStart = false;
      }
      const char c = _hist[(_histStart + i) % _histCap];
      replay += c;
      if (c == '\n') lineStart = true;
    }
    _unlock();
    client->text(replay);
    return;
  }

  if (type == WS_EVT_DATA) {
    AwsFrameInfo* info = (AwsFrameInfo*)arg;
    // Commands are short: only whole, unfragmented text frames are accepted.
    if (!info || !info->final || info->index != 0 || info->len != len || info->opcode != WS_TEXT) return;
    if (len == 0 || len > 1024) return;
    std::vector<uint8_t> buf(data, data + len);
    while (!buf.empty() && (buf.back() == 0 || buf.back() == '\n' || buf.back() == '\r')) buf.pop_back();
    const size_t n = buf.size();
    buf.push_back(0);
    if (_handler) _handler(buf.data(), n);
    if (_stringHandler) _stringHandler(String((const char*)buf.data()));
  }
}
