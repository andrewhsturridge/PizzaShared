// File: PizzaShared/src/PizzaNetCfg.cpp
#include "PizzaNetCfg.h"
#include <Preferences.h>
#include "BuildConfig.h"   // for WIFI_DEFAULT_SSID, WIFI_DEFAULT_PASS, OTA_BASE_URL_DEFAULT

namespace NetCfg {

static const char *kNs = "netcfg";

void compiledDefaults(Value &out) {
  memset(&out, 0, sizeof(out));
  strlcpy(out.ssid, WIFI_DEFAULT_SSID, sizeof(out.ssid));
  strlcpy(out.pass, WIFI_DEFAULT_PASS, sizeof(out.pass));
  strlcpy(out.base, OTA_BASE_URL_DEFAULT, sizeof(out.base));
}

void load(Value &out) {
  Preferences p;
  if (!p.begin(kNs, true)) { compiledDefaults(out); return; }
  size_t nS = p.getBytesLength("ssid");
  size_t nP = p.getBytesLength("pass");
  size_t nB = p.getBytesLength("base");
  if (!nS && !nP && !nB) { p.end(); compiledDefaults(out); return; }

  memset(&out, 0, sizeof(out));
  if (nS) p.getBytes("ssid", out.ssid, min(nS, sizeof(out.ssid)));
  if (nP) p.getBytes("pass", out.pass, min(nP, sizeof(out.pass)));
  if (nB) p.getBytes("base", out.base, min(nB, sizeof(out.base)));
  p.end();
  // Ensure NUL
  out.ssid[sizeof(out.ssid)-1] = 0;
  out.pass[sizeof(out.pass)-1] = 0;
  out.base[sizeof(out.base)-1] = 0;
}

bool save(const Value &v) {
  Preferences p;
  if (!p.begin(kNs, false)) return false;
  p.putBytes("ssid", v.ssid, strnlen(v.ssid, sizeof(v.ssid)) + 1);
  p.putBytes("pass", v.pass, strnlen(v.pass, sizeof(v.pass)) + 1);
  p.putBytes("base", v.base, strnlen(v.base, sizeof(v.base)) + 1);
  p.end();
  return true;
}

} // namespace NetCfg
