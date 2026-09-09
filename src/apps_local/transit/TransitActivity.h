#pragma once

#include <cstdint>
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
  struct Departure {
    char iso[26] = {};
    char label[24] = {};
  };

  struct Direction {
    const char* heading = "";
    Departure rows[4] = {};
    int count = 0;
  };

  enum class Phase : uint8_t { Ready, Busy, Notice };

  void chooseTab(int tab);
  void ensureConnected();
  void onWifiChosen(bool connected);
  bool fetchCurrent();
  bool fetchDirection(const char* timingPointCode, const char* line, const char* destination, Direction& out);
  bool tramDay() const;
  void setNotice(const char* text);

  int selectedTab_ = 0;
  Direction directions_[2];
  Phase phase_ = Phase::Ready;
  bool pendingFetch_ = false;
  bool interactionsReady_ = false;
  bool backPressSeen_ = false;
  char notice_[128] = {};
  char updated_[12] = {};
  toybox::Interactions interactions_;
};
