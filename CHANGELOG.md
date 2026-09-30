# Changelog

## 1.1.1 (2026-09-30)

- License changed from PolyForm Noncommercial 1.0.0 to **MIT**, so SKL-WebConsole can be used in commercial projects too. No code changes. Releases up to 1.1.0 remain under their original license.

## 1.1.0 (2026-09-30)

- Download menu with four formats: Log (`.log`), CSV (`.csv`), JSON (`.json`) and JSON Lines (`.jsonl`).
- "Only lines matching the filter" option exports just the lines you're looking at. The page remembers this choice and your last format.
- Each exported line carries its device sequence number, receive time, source (history or live) and level.
- CSV is Excel-friendly: UTF-8 byte-order mark and protection against spreadsheet formula injection.

## 1.0.0 (2026-09-28)

- First release: live browser console for ESP32 with pause, filter, level colors, commands, quick-command buttons, on-device history replay, sign-in options and `.log` download.
