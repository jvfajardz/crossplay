#pragma once

#include <cstdint>
#include <ctime>
#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"

class TransitActivity final : public Activity {
 public:
  TransitActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Transit", renderer, mappedInput) {}
  ~TransitActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  static constexpr int kMaxDepartures = 5;
  static constexpr int kMaxDirections = 8;

  struct Departure {
    char iso[26] = {};
    char label[24] = {};
    std::time_t expected = 0;
    bool delayed = false;
    bool cancelled = false;
  };

  struct Direction {
    const char* heading = "";
    Departure rows[kMaxDepartures] = {};
    int count = 0;
  };

  enum class Phase : uint8_t { Ready, Busy, Notice };

  void prepareDirections();
  void ensureConnected();
  void onWifiChosen(bool connected);
  bool fetchCurrent();
  bool fetchOvapi(const char* timingPointCode, const char* line, const char* destination, Direction& out);
  bool fetchTransitous(const char* stopId, const char* route, const char* headsign, std::time_t notBefore,
                       int resultLimit, Direction& out);
  bool saturday() const;
  void setNotice(const char* text);

  Direction directions_[kMaxDirections];
  int directionCount_ = 0;
  Phase phase_ = Phase::Ready;
  bool pendingFetch_ = false;
  bool interactionsReady_ = false;
  bool backPressSeen_ = false;
  char notice_[128] = {};
  char updated_[12] = {};
  toybox::Interactions interactions_;
};
