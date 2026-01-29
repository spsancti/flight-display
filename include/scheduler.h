#pragma once

#include <Arduino.h>

struct Task {
  uint32_t periodMs;
  uint32_t nextAt;
  void (*fn)();
};

inline void runTasks(Task *tasks, size_t n) {
  const uint32_t now = millis();
  for (size_t i = 0; i < n; i++) {
    if ((int32_t)(now - tasks[i].nextAt) >= 0) {
      tasks[i].fn();
      tasks[i].nextAt = now + tasks[i].periodMs;
    }
  }
}
