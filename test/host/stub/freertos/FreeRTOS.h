#pragma once
#include <mutex>
typedef std::recursive_mutex* SemaphoreHandle_t;
struct portMUX_TYPE { std::mutex m; };
#define portMUX_INITIALIZER_UNLOCKED {}
#define portENTER_CRITICAL(x) (x)->m.lock()
#define portEXIT_CRITICAL(x) (x)->m.unlock()
#define portMAX_DELAY 0xffffffff
