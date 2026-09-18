#include "TransitActivity.h"

#include <ArduinoJson.h>
#include <Logging.h>
#include <Memory.h>
#include <WiFi.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

#include "../../SilentRestart.h"
#include "../../activities/network/WifiSelectionActivity.h"
#include "../../fontIds.h"
#include "../../network/HttpDownloader.h"
#include "../Shelf.h"
#include "../ui/Toybox.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxTheme.h"
#include "DevMode.h"
#include "TransitScreens.h"

namespace {

struct OvapiSpec {
  const char* heading;
  const char* timingPoint;
  const char* line;
  const char* destination;
};

struct TransitousSpec {
  const char* heading;
  const char* stopId;
  const char* route;
  const char* headsign;
};

constexpr OvapiSpec kCore[2] = {
    {"306: Koogsingel -> Noord", "37402010", "306", "Amsterdam Noord"},
    {"306: Noord -> Koogsingel", "30001311", "306", "Purmerend Overwhere"},
};

constexpr TransitousSpec kMetro52[2] = {
    {"M52: Noord -> Zuid", "nl-OpenOV_NL:S:30009571", "52", "Zuid"},
    {"M52: Zuid -> Noord", "nl-OpenOV_NL:S:30007408", "52", "Noord"},
};

constexpr TransitousSpec kBus37[2] = {
    {"37: Noord -> Amstelstation", "nl-OpenOV_NL:S:30001314", "37", "Amstelstation"},
    {"37: Amstelstation -> Noord", "nl-OpenOV_NL:S:30009018", "37", "Station Noord"},
};

constexpr OvapiSpec kTram26[2] = {
    {"T26: Centraal -> Diemerparklaan", "30005029", "26", "IJburg"},
    {"T26: Diemerparklaan -> Centraal", "30008248", "26", "Centraal Station"},
};

constexpr TransitousSpec kNs[2] = {
    {"NS: Overwhere -> Sloterdijk", "nl-OpenOV_stoparea:18008", "Sprinter", "Hoofddorp"},
    {"NS: Sloterdijk -> Purmerend Overwhere", "nl-OpenOV_stoparea:18177", "Sprinter", "Hoorn Kersenboogerd"},
};

constexpr size_t kMaxResponseBytes = 256u * 1024u;

bool sameText(const char* left, const char* right) {
  return left != nullptr && right != nullptr && std::strcmp(left, right) == 0;
}

int64_t daysFromCivil(int year, const unsigned month, const unsigned day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yearOfEra = static_cast<unsigned>(year - era * 400);
  const unsigned dayOfYear = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
  return era * 146097 + static_cast<int>(dayOfEra) - 719468;
}

int amsterdamOffsetSeconds(std::time_t utc);

std::time_t parseIso(const char* iso) {
  if (iso == nullptr || std::strlen(iso) < 16) return 0;
  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;
  if (std::sscanf(iso, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second) < 5) return 0;
  std::time_t parsed =
      static_cast<std::time_t>(daysFromCivil(year, static_cast<unsigned>(month), static_cast<unsigned>(day)) * 86400 +
                               hour * 3600 + minute * 60 + second);
  const size_t length = std::strlen(iso);
  if (length > 19 && (iso[19] == 'Z' || iso[19] == 'z')) return parsed;
  for (size_t i = 19; i + 5 < length; ++i) {
    if (iso[i] != '+' && iso[i] != '-') continue;
    int offsetHours = 0;
    int offsetMinutes = 0;
    if (std::sscanf(iso + i + 1, "%d:%d", &offsetHours, &offsetMinutes) != 2) break;
    const int offset = (offsetHours * 60 + offsetMinutes) * 60;
    return iso[i] == '+' ? parsed - offset : parsed + offset;
  }
  // OVapi emits local Europe/Amsterdam timestamps without an offset.
  return parsed - amsterdamOffsetSeconds(parsed - 3600);
}

unsigned lastSunday(const int year, const unsigned month) {
  static constexpr unsigned kMonthDays[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  unsigned lastDay = kMonthDays[month];
  if (month == 2 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))) ++lastDay;
  const int64_t days = daysFromCivil(year, month, lastDay);
  const unsigned weekday = static_cast<unsigned>((days + 4) % 7);
  return lastDay - weekday;
}

int amsterdamOffsetSeconds(const std::time_t utc) {
  const std::tm* value = std::gmtime(&utc);
  if (value == nullptr) return 3600;
  const int year = value->tm_year + 1900;
  const std::time_t start = static_cast<std::time_t>(daysFromCivil(year, 3, lastSunday(year, 3)) * 86400 + 3600);
  const std::time_t end = static_cast<std::time_t>(daysFromCivil(year, 10, lastSunday(year, 10)) * 86400 + 3600);
  return utc >= start && utc < end ? 7200 : 3600;
}

bool amsterdamTime(const std::time_t utc, std::tm& out) {
  const std::time_t localEpoch = utc + amsterdamOffsetSeconds(utc);
  const std::tm* value = std::gmtime(&localEpoch);
  if (value == nullptr) return false;
  out = *value;
  return true;
}

void formatLabel(char* target, const size_t size, const std::time_t scheduled, const std::time_t expected) {
  std::tm local{};
  if (!amsterdamTime(scheduled, local)) {
    std::snprintf(target, size, "--:--");
    return;
  }
  const int delayMinutes = expected > scheduled ? static_cast<int>((expected - scheduled + 30) / 60) : 0;
  if (delayMinutes > 0) {
    std::snprintf(target, size, "%02d:%02d+%d", local.tm_hour, local.tm_min, delayMinutes);
  } else {
    std::snprintf(target, size, "%02d:%02d", local.tm_hour, local.tm_min);
  }
}

}  // namespace

std::unique_ptr<Activity> TransitActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<TransitActivity>(renderer, mappedInput);
}

void TransitActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  prepareDirections();
  ensureConnected();
}

void TransitActivity::onExit() {
  Activity::onExit();
  if (WiFi.getMode() != WIFI_MODE_NULL && !devmode::holdsRadio()) {
    WiFi.disconnect(false);
    delay(30);
    silentRestart();
  }
}

bool TransitActivity::saturday() const {
  const std::time_t now = std::time(nullptr);
  if (now <= 0) return false;
  std::tm local{};
  return amsterdamTime(now, local) && local.tm_wday == 6;
}

void TransitActivity::prepareDirections() {
  directionCount_ = 8;
  for (Direction& direction : directions_) direction = Direction{};
  for (int i = 0; i < 2; ++i) directions_[i].heading = kCore[i].heading;
  directions_[2].heading = kMetro52[0].heading;
  directions_[3].heading = kMetro52[1].heading;
  directions_[4].heading = kBus37[0].heading;
  directions_[5].heading = kBus37[1].heading;
  const bool tram = saturday();
  directions_[6].heading = tram ? kTram26[0].heading : kNs[0].heading;
  directions_[7].heading = tram ? kTram26[1].heading : kNs[1].heading;
}

void TransitActivity::setNotice(const char* text) {
  std::snprintf(notice_, sizeof(notice_), "%s", text == nullptr ? "" : text);
  phase_ = Phase::Notice;
}

void TransitActivity::ensureConnected() {
  if (WiFi.status() == WL_CONNECTED) {
    phase_ = Phase::Busy;
    pendingFetch_ = true;
    requestUpdate();
    return;
  }
  backPressSeen_ = false;
  WiFi.mode(WIFI_STA);
  auto picker = makeUniqueNoThrow<WifiSelectionActivity>(renderer, mappedInput);
  if (!picker) {
    setNotice("NOT ENOUGH MEMORY TO OPEN WI-FI.");
    requestUpdate();
    return;
  }
  startActivityForResult(std::move(picker),
                         [this](const ActivityResult& result) { onWifiChosen(!result.isCancelled); });
}

void TransitActivity::onWifiChosen(const bool connected) {
  if (!connected) {
    requestUpdate();
    return;
  }
  phase_ = Phase::Busy;
  pendingFetch_ = true;
  requestUpdate();
}

void TransitActivity::loop() {
  namespace fui = freeink::ui;
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) backPressSeen_ = true;
  if (pendingFetch_) {
    pendingFetch_ = false;
    if (!fetchCurrent()) setNotice("NO LIVE DEPARTURES ARRIVED. CHECK WI-FI OR TRY AGAIN IN A MOMENT.");
    requestUpdate();
    return;
  }
  if (backPressSeen_ && mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    backPressSeen_ = false;
    shelf::leave(renderer, mappedInput);
    return;
  }

  int tapX = 0;
  int tapY = 0;
  if (!mappedInput.wasScreenTapped(tapX, tapY) || !interactionsReady_) return;
  fui::InputSnapshot input;
  input.touchReleased = true;
  input.touchX = static_cast<int16_t>(tapX);
  input.touchY = static_cast<int16_t>(tapY);
  if (interactions_.route(input).action == transitui::ActionRefresh) ensureConnected();
}

bool TransitActivity::fetchOvapi(const char* timingPointCode, const char* line, const char* destination,
                                 Direction& out) {
  char url[96];
  std::snprintf(url, sizeof(url), "http://v0.ovapi.nl/tpc/%s/departures", timingPointCode);
  std::string response;
  response.reserve(12u * 1024u);
  bool overLimit = false;
  const bool fetched = HttpDownloader::fetchUrl(url, [&response, &overLimit](const uint8_t* data, const size_t len) {
    if (response.size() + len > kMaxResponseBytes) {
      overLimit = true;
      return false;
    }
    response.append(reinterpret_cast<const char*>(data), len);
    return true;
  });
  if (!fetched || overLimit) return false;

  JsonDocument doc;
  if (deserializeJson(doc, response)) return false;
  out.count = 0;
  const JsonObjectConst passes = doc[timingPointCode]["Passes"].as<JsonObjectConst>();
  for (JsonPairConst pair : passes) {
    const JsonObjectConst pass = pair.value().as<JsonObjectConst>();
    if (!sameText(pass["LinePublicNumber"] | "", line) || !sameText(pass["DestinationName50"] | "", destination))
      continue;
    const char* scheduledIso = pass["TargetDepartureTime"] | "";
    const char* expectedIso = pass["ExpectedDepartureTime"] | scheduledIso;
    const std::time_t scheduled = parseIso(scheduledIso);
    const std::time_t expected = parseIso(expectedIso);
    if (scheduled == 0 || expected == 0) continue;

    Departure candidate;
    std::snprintf(candidate.iso, sizeof(candidate.iso), "%s", expectedIso);
    candidate.expected = expected;
    candidate.cancelled = sameText(pass["TripStopStatus"] | "", "CANCEL");
    candidate.delayed = expected > scheduled + 30;
    formatLabel(candidate.label, sizeof(candidate.label), scheduled, expected);

    int insert = out.count;
    for (int i = 0; i < out.count; ++i) {
      if (candidate.expected < out.rows[i].expected) {
        insert = i;
        break;
      }
    }
    if (insert >= kMaxDepartures) continue;
    const int last = std::min(out.count, kMaxDepartures - 1);
    for (int i = last; i > insert; --i) out.rows[i] = out.rows[i - 1];
    out.rows[insert] = candidate;
    if (out.count < kMaxDepartures) ++out.count;
  }
  return true;
}

bool TransitActivity::fetchTransitous(const char* stopId, const char* route, const char* headsign,
                                      const std::time_t notBefore, const int resultLimit, Direction& out) {
  char url[192];
  if (notBefore > 0) {
    std::tm utc{};
    const std::tm* value = std::gmtime(&notBefore);
    if (value == nullptr) return false;
    utc = *value;
    std::snprintf(
        url, sizeof(url),
        "https://api.transitous.org/api/v1/stoptimes?stopId=%s&n=%d&time=%04d-%02d-%02dT%02d%%3A%02d%%3A%02dZ", stopId,
        resultLimit, utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, utc.tm_hour, utc.tm_min, utc.tm_sec);
  } else {
    std::snprintf(url, sizeof(url), "https://api.transitous.org/api/v1/stoptimes?stopId=%s&n=%d", stopId, resultLimit);
  }
  std::string response;
  response.reserve(24u * 1024u);
  bool overLimit = false;
  const bool fetched = HttpDownloader::fetchUrl(url, [&response, &overLimit](const uint8_t* data, const size_t len) {
    if (response.size() + len > kMaxResponseBytes) {
      overLimit = true;
      return false;
    }
    response.append(reinterpret_cast<const char*>(data), len);
    return true;
  });
  if (!fetched || overLimit) return false;

  JsonDocument filter;
  filter["stopTimes"][0]["routeShortName"] = true;
  filter["stopTimes"][0]["headsign"] = true;
  filter["stopTimes"][0]["place"]["scheduledDeparture"] = true;
  filter["stopTimes"][0]["place"]["departure"] = true;
  filter["stopTimes"][0]["cancelled"] = true;
  filter["stopTimes"][0]["tripCancelled"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, response, DeserializationOption::Filter(filter))) return false;
  out.count = 0;
  for (JsonObjectConst item : doc["stopTimes"].as<JsonArrayConst>()) {
    if (!sameText(item["routeShortName"] | "", route) || !sameText(item["headsign"] | "", headsign)) continue;
    const char* scheduledIso = item["place"]["scheduledDeparture"] | "";
    const char* expectedIso = item["place"]["departure"] | scheduledIso;
    const std::time_t scheduled = parseIso(scheduledIso);
    const std::time_t expected = parseIso(expectedIso);
    if (scheduled == 0 || expected == 0 || expected < notBefore) continue;

    Departure candidate;
    std::snprintf(candidate.iso, sizeof(candidate.iso), "%s", expectedIso);
    candidate.expected = expected;
    candidate.cancelled = (item["cancelled"] | false) || (item["tripCancelled"] | false);
    candidate.delayed = expected > scheduled + 30;
    formatLabel(candidate.label, sizeof(candidate.label), scheduled, expected);
    if (out.count < kMaxDepartures) out.rows[out.count++] = candidate;
  }
  return true;
}

bool TransitActivity::fetchCurrent() {
  prepareDirections();
  bool anyOk = false;
  for (int i = 0; i < 2; ++i) {
    anyOk = fetchOvapi(kCore[i].timingPoint, kCore[i].line, kCore[i].destination, directions_[i]) || anyOk;
  }
  for (int i = 0; i < 2; ++i) {
    anyOk = fetchTransitous(kMetro52[i].stopId, kMetro52[i].route, kMetro52[i].headsign, 0, 30, directions_[i + 2]) ||
            anyOk;
  }

  std::time_t noordArrival = 0;
  for (int i = 0; i < directions_[0].count; ++i) {
    if (!directions_[0].rows[i].cancelled) {
      noordArrival = directions_[0].rows[i].expected + 30 * 60;
      break;
    }
  }
  anyOk =
      fetchTransitous(kBus37[0].stopId, kBus37[0].route, kBus37[0].headsign, noordArrival, 30, directions_[4]) || anyOk;
  anyOk = fetchTransitous(kBus37[1].stopId, kBus37[1].route, kBus37[1].headsign, 0, 30, directions_[5]) || anyOk;

  if (saturday()) {
    for (int i = 0; i < 2; ++i)
      anyOk = fetchOvapi(kTram26[i].timingPoint, kTram26[i].line, kTram26[i].destination, directions_[i + 6]) || anyOk;
  } else {
    for (int i = 0; i < 2; ++i)
      anyOk = fetchTransitous(kNs[i].stopId, kNs[i].route, kNs[i].headsign, 0, i == 0 ? 30 : 90, directions_[i + 6]) ||
              anyOk;
  }
  if (!anyOk) return false;

  const std::time_t now = std::time(nullptr);
  std::tm local{};
  if (amsterdamTime(now, local)) std::snprintf(updated_, sizeof(updated_), "%02d:%02d", local.tm_hour, local.tm_min);
  phase_ = Phase::Ready;
  notice_[0] = '\0';
  return true;
}

void TransitActivity::render(RenderLock&&) {
  namespace fui = freeink::ui;
  renderer.clearScreen();
  const toybox::Faces faces{UI_10_FONT_ID, UI_12_FONT_ID, NOTOSANS_12_FONT_ID};
  fui::GfxRendererTarget target = toybox::makeTarget(renderer, faces);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady_ = false;
  toybox::Frame frame(target, device, noInput, interactions_);
  toybox::Screen screen(frame);

  transitui::Model model;
  model.updated = updated_;
  char current[12] = {};
  const std::time_t now = std::time(nullptr);
  std::tm local{};
  if (amsterdamTime(now, local)) std::snprintf(current, sizeof(current), "%02d:%02d", local.tm_hour, local.tm_min);
  model.current = current;
  model.directionCount = directionCount_;
  if (phase_ == Phase::Busy) {
    model.notice = "FETCHING LIVE DEPARTURES...";
  } else if (phase_ == Phase::Notice) {
    model.notice = notice_;
  } else {
    for (int d = 0; d < directionCount_; ++d) {
      model.directions[d].heading = directions_[d].heading;
      model.directions[d].count = directions_[d].count;
      for (int i = 0; i < directions_[d].count; ++i) {
        model.directions[d].times[i].label = directions_[d].rows[i].label;
        model.directions[d].times[i].highlighted =
            i == 0 || directions_[d].rows[i].delayed || directions_[d].rows[i].cancelled;
        model.directions[d].times[i].cancelled = directions_[d].rows[i].cancelled;
      }
    }
  }
  transitui::build(screen, model);

  interactionsReady_ = true;
  toybox::reportOverflow(interactions_, "Transit");
  renderer.displayBuffer();
}
