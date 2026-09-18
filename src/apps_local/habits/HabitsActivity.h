#pragma once

#include <memory>

#include "../../activities/Activity.h"
#include "../../components/OptionPopup.h"
#include "HabitModel.h"

class HabitsActivity final : public Activity {
 public:
  HabitsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Habits", renderer, mappedInput) {}
  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  uint32_t surfaceMeaning() const override;

 private:
  enum class View : uint8_t { Habits, Detail, Schedule, Stats, Error };

  bool load();
  bool save();
  void editName(bool task, habits::Id id, const std::string& initial);
  void addHabit();
  void addTask();
  void showHabitMenu(habits::Id id);
  void showTaskMenu(habits::Id id);
  void showDateMenu(int day);
  void changeMonth(int delta);
  void selectDateAt(int x, int y);
  int today() const;
  habits::Id selectedHabitId() const;
  habits::Id selectedTaskId() const;
  uint8_t selectedScheduleMask() const;
  void clampSelections();
  void markDirty(bool immediate = false);

  void drawHabits();
  void drawDetail();
  void drawSchedule();
  void drawStats();
  void drawDayMark(int x, int y, int size, habits::DayState state) const;
  void drawTextFit(int font, int x, int y, int width, const std::string& text, bool ink = true) const;

  habits::Model model_;
  OptionPopup popup_;
  View view_ = View::Habits;
  int habitRow_ = 0;
  int taskRow_ = 0;
  int taskScroll_ = 0;
  int selectedDay_ = 0;
  int shownYear_ = 0;
  unsigned shownMonth_ = 1;
  uint8_t editingMask_ = habits::kAllWeekdays;
  int scheduleRow_ = 0;
  bool dirty_ = false;
  bool clockValid_ = true;
  std::string error_;

  int holdX_ = -1;
  int holdY_ = -1;
  unsigned long holdSince_ = 0;
  bool holdFired_ = false;
};
