// Host tests: compiles the real src/SKLWebConsole.cpp against stub Arduino /
// ESPAsyncWebServer / FreeRTOS headers (test/host/stub). Run: sh test/host/run.sh
#include "SKLWebConsolePage.h"
#include "SKLWebConsole.h"
#include <thread>
#include <cassert>
#include <iostream>
#include <sstream>
unsigned long millis() { static unsigned long t = 0; return t += 1000; }
static int fails = 0;
#define CHECK(c, m) do { if (c) std::cout << "PASS " << m << "\n"; else { std::cout << "FAIL " << m << "\n"; fails++; } } while (0)

static std::vector<std::string> split(const std::string& s) { std::vector<std::string> v; std::stringstream ss(s); std::string l; while (std::getline(ss, l)) if (!l.empty()) v.push_back(l); return v; }

int main() {
  SKLWebConsole c;
  // 1. output before begin() is kept for replay
  c.println("[INFO] boot 1");
  c.print("[INFO] boot "); c.print(2); c.println();
  c.printf("[WARN] boot %d\r\n", 3);
  AsyncWebServer server;
  c.setTitle("Gretta \"test\"");
  c.addQuickCommand("Status", "status");
  bool authed = true;
  c.setAuthCheck([&](AsyncWebServerRequest*) { return authed; }, "/admin");
  c.begin(&server, "/webserial/");
  auto* ws = (AsyncWebSocket*)server.handlers.at(0);
  CHECK(ws->url == "/webserial/ws", "ws path, trailing slash trimmed");
  CHECK(server.routes.size() == 2 && server.routes[0].first == "/webserial/auth" && server.routes[1].first == "/webserial", "routes registered specific-first");

  AsyncWebSocketClient cl;
  ws->ev(ws, &cl, WS_EVT_CONNECT, nullptr, nullptr, 0);
  CHECK(cl.frames.size() == 2, "hello + replay on connect");
  CHECK(cl.frames[0][0] == 1 && cl.frames[0].find("\"title\":\"Gretta \\\"test\\\"\"") != std::string::npos, "hello JSON escapes title");
  CHECK(cl.frames[0].find("\"hist\":3") != std::string::npos && cl.frames[0].find("{\"l\":\"Status\",\"c\":\"status\"}") != std::string::npos, "hello has hist count + quick commands");
  auto r = split(cl.frames[1].substr(1));
  CHECK(r.size() == 3 && r[0] == "0\t[INFO] boot 1" && r[1] == "1\t[INFO] boot 2" && r[2] == "2\t[WARN] boot 3", "replay has numbered lines, \\r stripped");

  // 2. live lines only go out when a client is connected
  c.println("no clients yet");
  CHECK(ws->all.empty(), "no send with zero clients");
  ws->clients.push_back(&cl);
  c.println("hello live");
  CHECK(ws->all.size() == 1 && ws->all[0] == "4\thello live\n", "live frame carries seq");
  c.print("part A, "); c.print("part B");
  CHECK(ws->all.size() == 1, "partial line held until newline");
  c.println();
  CHECK(ws->all.back() == "5\tpart A, part B\n", "partial writes joined");

  // 3. long lines split at LINE_MAX
  std::string big(1200, 'x');
  c.println(big.c_str());
  size_t total = 0; for (auto& f : ws->all) for (auto& l : split(f)) if (l.find('x') != std::string::npos) total += l.size() - l.find('\t') - 1;
  CHECK(total == 1200, "long line split without losing bytes");

  // 4. history evicts oldest whole lines within the cap, seq stays right
  for (int i = 0; i < 100; i++) c.printf("[DEBUG] filler line %03d\n", i);
  AsyncWebSocketClient cl2;
  ws->ev(ws, &cl2, WS_EVT_CONNECT, nullptr, nullptr, 0);
  auto h = split(cl2.frames[1].substr(1));
  CHECK(cl2.frames[1].size() <= 256 + h.size() * 8 + 1, "history bounded by cap");
  CHECK(!h.empty() && h.back().find("filler line 099") != std::string::npos, "newest line kept");
  bool contiguous = true; long prev = -1;
  for (auto& l : h) { long s = std::stol(l.substr(0, l.find('\t'))); if (prev >= 0 && s != prev + 1) contiguous = false; prev = s; }
  long lastLive = std::stol(split(ws->all.back()).back());
  CHECK(contiguous && prev == lastLive, "replay seqs contiguous and match live numbering");
  CHECK(h.front().find("filler") != std::string::npos && h.front().find('\t') != std::string::npos, "oldest kept line is whole");

  // 5. incoming commands
  std::string got; String got2;
  c.onMessage([&](uint8_t* d, size_t n) { got.assign((char*)d, n); assert(d[n] == 0); });
  c.onMessage([&](const String& s) { got2 = s; });
  AwsFrameInfo fi{}; fi.final = 1; fi.index = 0; fi.opcode = WS_TEXT;
  uint8_t msg[] = {'s','t','a','t','u','s','\0'}; fi.len = sizeof msg;
  ws->ev(ws, &cl, WS_EVT_DATA, &fi, msg, sizeof msg);
  CHECK(got == "status" && got2.s == "status", "command delivered, trailing NUL trimmed, NUL-terminated");
  got.clear(); fi.final = 0;
  ws->ev(ws, &cl, WS_EVT_DATA, &fi, msg, sizeof msg);
  CHECK(got.empty(), "fragmented frames ignored");

  // 6. auth
  AsyncWebServerRequest rq;
  CHECK(ws->hs(&rq), "handshake allowed when check passes");
  authed = false;
  CHECK(!ws->hs(&rq), "handshake refused when check fails");
  server.routes[0].second(&rq);
  CHECK(rq.sent && rq.sent->body == "{\"authed\":false,\"login\":\"/admin\"}", "auth endpoint JSON");
  AsyncWebServerRequest pg; server.routes[1].second(&pg);
  CHECK(pg.sent && pg.sent->headers["Content-Encoding"] == "gzip" && pg.sent->body.size() == SKL_WEBCONSOLE_PAGE_GZ_LEN, "page served gzipped");
  authed = true;
  c.setAuthentication("admin", "pw");
  AsyncWebServerRequest bad; bad.basicOk = false; server.routes[1].second(&bad);
  CHECK(bad.askedAuth && !bad.sent, "basic auth prompts on page");
  CHECK(!ws->hs(&bad), "basic auth enforced on socket");

  // 7. slow client: lines counted as dropped, not blocking
  ws->full = true; size_t before = ws->all.size();
  c.println("a"); c.println("b");
  CHECK(ws->all.size() == before && c.droppedLines() == 2, "full queue -> dropped count");
  ws->full = false;

  // 8. loop() cleanup throttled
  c.loop(); c.loop();
  CHECK(ws->cleanups >= 1, "loop cleans up clients");

  // 9. concurrent printers never splice lines
  ws->all.clear();
  auto worker = [&](char ch) { for (int i = 0; i < 2000; i++) { std::string s(40, ch); c.println(s.c_str()); } };
  std::thread t1(worker, 'A'), t2(worker, 'B'), t3([&] { for (int i = 0; i < 2000; i++) { c.print("CCCCCCCCCCCCCCCCCCCC"); c.printf("CCCCCCCCCCCCCCCCCCCC\n"); } });
  t1.join(); t2.join(); t3.join();
  int bad_lines = 0, n = 0;
  for (auto& f : ws->all) for (auto& l : split(f)) { n++; std::string x = l.substr(l.find('\t') + 1); if (x.size() != 40 || x.find_first_not_of(x[0]) != std::string::npos) bad_lines++; }
  CHECK(n == 6000 && bad_lines <= 2000, "6000 lines from 3 threads");
  std::cout << "  (spliced: " << bad_lines << " -- only print()+printf() pairs, which are two calls, may interleave)\n";
  int badAB = 0; for (auto& f : ws->all) for (auto& l : split(f)) { std::string x = l.substr(l.find('\t') + 1); if ((x[0]=='A'||x[0]=='B') && (x.size()!=40 || x.find_first_not_of(x[0]) != std::string::npos)) badAB++; }
  CHECK(badAB == 0, "println() lines are never spliced");
  std::cout << (fails ? "FAILURES: " : "ALL PASSED ") << fails << "\n";
  return fails != 0;
}
