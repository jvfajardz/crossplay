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
#include "../../network/HttpDownloader.h"
#include "../Shelf.h"
#include "../ui/Toybox.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxTheme.h"
#include "DevMode.h"
#include "TransitScreens.h"

namespace {

struct StopSpec {
  const char* heading;
  const char* timingPoint;
  const char* line;
  const char* destination;
};

constexpr StopSpec kStops[3][2] = {
    {{"TO AMSTERDAM", "37402010", "306", "Amsterdam Noord"},
     {"TO OVERWHERE", "37402240", "306", "Purmerend Overwhere"}},
    {{"NOORD TO ZUID", "30009571", "52", "Zuid"}, {"ZUID TO NOORD", "30007408", "52", "Noord"}},
    {{"CENTRAAL OUT", "30005029", "26", "IJburg"}, {"DIEMERPARKLAAN", "30008248", "26", "Centraal Station"}},
};

constexpr size_t kMaxResponseBytes = 64u * 1024u;

bool sameText(const char* left, const char* right) {
  return left != nullptr && right != nullptr && std::strcmp(left, right) == 0;
}

}  // namespace

std::unique_ptr<Activity> TransitActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<TransitActivity>(renderer, mappedInput);
}

void TransitActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  chooseTab(0);
}

void TransitActivity::onExit() {
  Activity::onExit();
  if (WiFi.getMode() != WIFI_MODE_NULL && !devmode::holdsRadio()) {
    WiFi.disconnect(false);
    delay(30);
    silentRestart();
  }
}

void TransitActivity::chooseTab(const int tab) {
  if (tab < 0 || tab > 2) return;
  selectedTab_ = tab;
  directions_[0] = Direction{};
  directions_[1] = Direction{};
  directions_[0].heading = kStops[tab][0].heading;
  directions_[1].heading = kStops[tab][1].heading;
  notice_[0] = '\0';
  phase_ = Phase::Ready;
  if (tab == 2 && !tramDay()) {
    setNotice("TRAM 26 IS SHOWN ON SATURDAYS. COME BACK THEN FOR LIVE DEPARTURES.");
  }
  requestUpdate();
}

bool TransitActivity::tramDay() const {
  const std::time_t now = std::time(nullptr);
  if (now <= 0) return false;
  const std::tm* local = std::localtime(&now);
  return local != nullptr && local->tm_wday == 6;
}

void TransitActivity::setNotice(const char* text) {
  std::snprintf(notice_, sizeof(notice_), "%s", text == nullptr ? "" : text);
  phase_ = Phase::Notice;
}

void TransitActivity::ensureConnected() {
  if (selectedTab_ == 2 && !tramDay()) {
    setNotice("TRAM 26 IS SHOWN ON SATURDAYS. COME BACK THEN FOR LIVE DEPARTURES.");
    requestUpdate();
    return;
  }
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
  const fui::ActionEvent event = interactions_.route(input);
  switch (event.action) {
    case transitui::ActionBus306:
      chooseTab(0);
      break;
    case transitui::ActionMetro52:
      chooseTab(1);
      break;
    case transitui::ActionTram26:
      chooseTab(2);
      break;
    case transitui::ActionRefresh:
      ensureConnected();
      break;
    default:
      break;
  }
}

bool TransitActivity::fetchDirection(const char* timingPointCode, const char* line, const char* destination,
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
  if (!fetched || overLimit) {
    LOG_ERR("TRANSIT", "fetch failed for %s", timingPointCode);
    return false;
  }

  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, response);
  if (error) {
    LOG_ERR("TRANSIT", "JSON failed for %s: %s", timingPointCode, error.c_str());
    return false;
  }

  out.count = 0;
  const JsonObjectConst passes = doc[timingPointCode]["Passes"].as<JsonObjectConst>();
  for (JsonPairConst pair : passes) {
    const JsonObjectConst pass = pair.value().as<JsonObjectConst>();
    if (!sameText(pass["LinePublicNumber"] | "", line)) continue;
    if (!sameText(pass["DestinationName50"] | "", destination)) continue;
    const char* expected = pass["ExpectedDepartureTime"] | pass["TargetDepartureTime"] | "";
    if (std::strlen(expected) < 16) continue;

    Departure candidate;
    std::snprintf(candidate.iso, sizeof(candidate.iso), "%s", expected);
    const char* status = pass["TripStopStatus"] | "";
    if (sameText(status, "CANCEL")) {
      std::snprintf(candidate.label, sizeof(candidate.label), "%.5s  CANCELLED", expected + 11);
    } else {
      std::snprintf(candidate.label, sizeof(candidate.label), "%.5s", expected + 11);
    }

    int insert = out.count;
    for (int i = 0; i < out.count; ++i) {
      if (std::strcmp(candidate.iso, out.rows[i].iso) < 0) {
        insert = i;
        break;
      }
    }
    if (insert >= 4) continue;
    const int last = std::min(out.count, 3);
    for (int i = last; i > insert; --i) out.rows[i] = out.rows[i - 1];
    out.rows[insert] = candidate;
    if (out.count < 4) ++out.count;
  }
  return true;
}

bool TransitActivity::fetchCurrent() {
  const StopSpec& first = kStops[selectedTab_][0];
  const StopSpec& second = kStops[selectedTab_][1];
  const bool firstOk = fetchDirection(first.timingPoint, first.line, first.destination, directions_[0]);
  const bool secondOk = fetchDirection(second.timingPoint, second.line, second.destination, directions_[1]);
  if (!firstOk && !secondOk) return false;

  const std::time_t now = std::time(nullptr);
  const std::tm* local = std::localtime(&now);
  if (local != nullptr) std::snprintf(updated_, sizeof(updated_), "%02d:%02d", local->tm_hour, local->tm_min);
  if (directions_[0].count == 0 && directions_[1].count == 0) {
    setNotice("NO DEPARTURES ARE LISTED RIGHT NOW. SERVICE MAY BE FINISHED OR DISRUPTED.");
  } else {
    phase_ = Phase::Ready;
    notice_[0] = '\0';
  }
  return true;
}

void TransitActivity::render(RenderLock&&) {
  namespace fui = freeink::ui;

  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer, toybox::readingFaces());
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady_ = false;
  toybox::Frame frame(target, device, noInput, interactions_);
  toybox::Screen screen(frame);

  transitui::Model model;
  model.selectedTab = selectedTab_;
  model.updated = updated_;
  if (phase_ == Phase::Busy) {
    model.notice = "FETCHING LIVE DEPARTURES...";
  } else if (phase_ == Phase::Notice) {
    model.notice = notice_;
  } else {
    for (int d = 0; d < 2; ++d) {
      model.directions[d].heading = directions_[d].heading;
      model.directions[d].count = directions_[d].count;
      for (int i = 0; i < directions_[d].count; ++i) model.directions[d].times[i] = directions_[d].rows[i].label;
    }
  }
  transitui::build(screen, model);

  interactionsReady_ = true;
  toybox::reportOverflow(interactions_, "Transit");
  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
