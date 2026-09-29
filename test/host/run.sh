#!/bin/sh
# Host unit tests (no ESP32 needed): needs g++ with C++17.
set -e
cd "$(dirname "$0")"
g++ -std=gnu++17 -Wall -Wextra -Wno-unused-parameter -DSKL_WEBCONSOLE_HISTORY_BYTES=256 \
  -Istub -I../../src console_test.cpp ../../src/SKLWebConsole.cpp -pthread -o console_test
./console_test
