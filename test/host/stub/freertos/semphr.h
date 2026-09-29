#pragma once
#include "FreeRTOS.h"
inline SemaphoreHandle_t xSemaphoreCreateRecursiveMutex() { return new std::recursive_mutex(); }
inline void vSemaphoreDelete(SemaphoreHandle_t m) { delete m; }
inline int xSemaphoreTakeRecursive(SemaphoreHandle_t m, uint32_t) { m->lock(); return 1; }
inline int xSemaphoreGiveRecursive(SemaphoreHandle_t m) { m->unlock(); return 1; }
