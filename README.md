# SKLWebConsole

A browser-based serial console for ESP32. Print to `WebConsole` the same way you print to `Serial`, then open `http://<device-ip>/webserial` to watch it live and send commands back.

Built by [Shady Knoll Labs](https://shadyknolllabs.com) for the Gnode plant sensor, and usable in any ESP32 + ESPAsyncWebServer project.

## Features

- **Pause and read.** Pause (or press Esc) freezes the view while lines keep arriving in the background. A "N new lines · Jump to live" button takes you back. Scrolling up to read, or selecting text, pauses on its own, and scrolling back to the bottom resumes.
- **See what happened before you opened the page.** The device keeps the last 8 KB of output, including boot messages, and replays it to every browser that connects.
- **No duplicates, no silent gaps.** Every line is numbered. Reconnecting never repeats lines, and if a slow link forces lines to be skipped, the page says how many.
- **Filtering.** Filter by text or `/regex/` with matches highlighted, and toggle ERR / WARN / INFO / DEBUG levels. Levels come from `[ERROR]`-style tags in your lines.
- **Commands.** A command box with ↑/↓ history (remembered in the browser), plus one-click quick-command buttons that the firmware defines.
- **Download in the format you need.** Save the session as a plain `.log`, a `.csv` for Excel or Google Sheets, `.json` for scripts, or `.jsonl` (JSON Lines) for tools like `jq` and pandas. Tick "Only lines matching the filter" to export just what you're looking at. You can also copy the visible lines.
- **Safe from any task.** `println()` sends each line whole, even when `loop()` and web handlers print at the same moment.
- **Optional sign-in.** Protect the console with HTTP Basic credentials or with your own check, such as an existing admin session.
- **Works offline.** The page is about 7 KB gzipped, served from flash, with no CDN or internet needed (it works on a device's own setup network). It's usable on a phone too.

## Install

PlatformIO, from GitHub:

```ini
lib_deps =
    https://github.com/mabbott2011/SKL-WebConsole.git#v1.0.0
```

Or copy this folder into your project's `lib/` directory. Dependencies are [ESP32Async/ESPAsyncWebServer](https://github.com/ESP32Async/ESPAsyncWebServer) 3.6+ and [ESP32Async/AsyncTCP](https://github.com/ESP32Async/AsyncTCP).

## Usage

```cpp
#include <SKLWebConsole.h>
AsyncWebServer server(80);

void setup() {
  WebConsole.setTitle("Kitchen sensor");
  WebConsole.addQuickCommand("Status", "status");
  WebConsole.onMessage([](const String& cmd) {
    if (cmd == "status") WebConsole.println("[INFO] all good");
  });
  WebConsole.begin(&server);            // page at /webserial
  server.begin();
}

void loop() {
  WebConsole.loop();                    // housekeeping, cheap
}
```

`WebConsole` is a `Print`, so `print`, `println` and `printf` all work. Output is captured from the very first print, even before `begin()`.

## API

| Call | What it does |
|---|---|
| `begin(server, path = "/webserial")` | Registers the page at `path`, the WebSocket at `path/ws` and the sign-in check at `path/auth`. |
| `onMessage(void(uint8_t* data, size_t len))` | Handler for typed commands. The data is NUL-terminated and has trailing newlines trimmed. |
| `onMessage(void(const String&))` | The same, as a String. |
| `setAuthentication(user, pass)` | HTTP Basic credentials. The browser prompts for them. |
| `setAuthCheck(fn, loginUrl)` | Your own check, e.g. `[](AsyncWebServerRequest* r){ return isSignedIn(r); }`. When it fails, the page shows a "Sign in required" card linking to `loginUrl`. |
| `setTitle(text)` / `setTitleProvider(fn)` | Page header. The provider is re-read on each connection. |
| `addQuickCommand(label, command)` | Adds a one-click button that sends `command`. |
| `setHistorySize(bytes)` | Size of the replay buffer (default 8192). Call it before the first print. `0` disables replay. |
| `setMaxClients(n)` | Maximum simultaneous browsers (default 3). The oldest connection is dropped. |
| `loop()` | Drops dead connections. Call it from `loop()`. |
| `clientCount()`, `droppedLines()` | Stats. |

Compile-time options: `SKL_WEBCONSOLE_LINE_MAX` (default 512) and `SKL_WEBCONSOLE_HISTORY_BYTES` (default 8192).

## Migrating from WebSerial

| WebSerial | SKLWebConsole |
|---|---|
| `#include <WebSerial.h>` | `#include <SKLWebConsole.h>` |
| `WebSerial.begin(&server)` | `WebConsole.begin(&server)`. Same default URL, `/webserial`. |
| `WebSerial.onMessage(handler)` | `WebConsole.onMessage(handler)`. Same handler signatures. |
| `WebSerial.print/println/printf` | `WebConsole.print/println/printf` |
| `WebSerial.setAuthentication(u, p)` | `WebConsole.setAuthentication(u, p)` |
| `WebSerial.loop()` | `WebConsole.loop()` |

## Export formats

Every export has the same fields for each line:

| Field | Meaning |
|---|---|
| `line` | Line number shown in the page |
| `seq` | The device's own line number (counts up from 0 at boot) |
| `received` | When the browser received the line (ISO 8601, UTC). CSV also has a `received_local` column in your time zone |
| `source` | `history` (replayed from the device's buffer when the page connected) or `live` |
| `level` | `CRITICAL`, `ERROR`, `WARN`, `INFO`, `DEBUG`, or empty if the line has no level tag |
| `message` | The line exactly as printed |

- **CSV** starts with a UTF-8 byte-order mark so Excel keeps symbols like µ and °. Messages starting with `=`, `+`, `-` or `@` get a leading `'` so spreadsheets don't run them as formulas; the other formats keep the text untouched.
- **JSON** wraps the lines with details about the export: device name, console version, export time, boot ID, and the filter used (if any).
- **JSON Lines** is one JSON object per line, with no wrapper, which is easy to stream, `grep` or load with `pandas.read_json(..., lines=True)`.

For history lines, `received` is when the page connected, not when the device printed the line. Use your own timestamps in the message if you need exact device times.

## Editing the page

The page's source is `page/console.html`. The firmware embeds a gzipped copy in `src/SKLWebConsolePage.h`, so after you edit the page, run:

```sh
python3 tools/embed_page.py
```

## Protocol (for other clients)

The WebSocket at `path/ws` sends text frames:

- `\x01{json}`: hello, with `title`, `version`, `boot` (random ID per boot), `hist` (number of replayed lines) and `commands` (`[{l, c}]`).
- `\x02` followed by `seq\ttext\n` lines: the history replay. It's always sent, even when empty.
- `seq\ttext\n` lines: live output. `seq` counts up from 0 at boot.

Frames sent from the page are plain text commands.

## License

PolyForm Noncommercial 1.0.0. See `LICENSE`.
