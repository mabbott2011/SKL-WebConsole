// SKLWebConsole basic example.
// Flash, open the Serial Monitor for the IP, then browse to http://<ip>/webserial
#include <WiFi.h>
#include <SKLWebConsole.h>

const char* WIFI_SSID = "your-network";
const char* WIFI_PASS = "your-password";

AsyncWebServer server(80);
unsigned long lastTick = 0;
int counter = 0;

void setup() {
  Serial.begin(115200);
  WebConsole.println("[INFO] Booting...");   // captured even before begin(); replayed later

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) delay(250);
  Serial.printf("Console: http://%s/webserial\n", WiFi.localIP().toString().c_str());

  WebConsole.setTitle("Example device");
  WebConsole.addQuickCommand("Ping", "ping");
  WebConsole.addQuickCommand("Heap", "heap");
  // Optional: WebConsole.setAuthentication("admin", "change-me");
  WebConsole.onMessage([](const String& cmd) {
    if (cmd == "ping") WebConsole.println("[INFO] pong");
    else if (cmd == "heap") WebConsole.printf("[INFO] Free heap: %u bytes\n", ESP.getFreeHeap());
    else WebConsole.println("[WARN] Unknown command: " + cmd);
  });
  WebConsole.begin(&server);
  server.begin();
  WebConsole.println("[INFO] Web console ready");
}

void loop() {
  WebConsole.loop();
  if (millis() - lastTick > 2000) {
    lastTick = millis();
    WebConsole.printf("[DEBUG] tick %d, uptime %lus\n", counter++, millis() / 1000);
  }
}
