// File: PizzaShared/include/PizzaNetCfg.h
#pragma once
#include <Arduino.h>

namespace NetCfg {
  // Max lengths (incl. NUL)
  static constexpr size_t MAX_SSID = 32;
  static constexpr size_t MAX_PASS = 64;
  static constexpr size_t MAX_BASE = 128;

  struct Value {
    char ssid[MAX_SSID];
    char pass[MAX_PASS];
    char base[MAX_BASE];
  };

  // Load from NVS; if nothing stored, fill with compiled defaults.
  void load(Value &out);

  // Save to NVS. Returns true on success.
  bool save(const Value &v);

  // Set compiled defaults for first boot
  void compiledDefaults(Value &out);
}
