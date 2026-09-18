#include "HabitsActivity.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <ctime>

#include "../../activities/util/KeyboardEntryActivity.h"
#include "../../components/UITheme.h"
#include "../Shelf.h"
#include "../ui/Toybox.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxTheme.h"
#include "HabitCsvStore.h"
#include "HabitStats.h"

namespace {
constexpr char kRoot[] = "/.crosspoint/habits";
constexpr const char* kFiles[] = {"habits.csv", "tasks.csv", "schedules.csv", "completions.csv", "exceptions.csv"};
constexpr unsigned long kHoldMs = 450;
constexpr int kRowH = 54;
constexpr int kCalendarTop = 72;
constexpr int kCalendarSide = 456;
constexpr int kCalendarLeft = 12;
constexpr int kCalendarHeader = 54;
constexpr int kWeekHeader = 28;

std::string pathFor(const char* name, const char* suffix = "") {
  return std::string(kRoot) + "/" + name + suffix;
}

int daysInMonth(const int year, const unsigned month) {
  int nextYear = year;
  unsigned nextMonth = month + 1;
  if (nextMonth == 13) {
    nextMonth = 1;
    ++nextYear;
  }
  return habits::daysFromCivil(nextYear, nextMonth, 1) - habits::daysFromCivil(year, month, 1);
}

const char* monthName(const unsigned month) {
  static const char* names[] = {"JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE",
                                "JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER"};
  return month >= 1 && month <= 12 ? names[month - 1] : "";
}
}  // namespace

std::unique_ptr<Activity> HabitsActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<HabitsActivity>(renderer, mappedInput);
}

int HabitsActivity::today() const {
  const time_t now = time(nullptr);
  tm parts{};
  localtime_r(&now, &parts);
  return habits::daysFromCivil(parts.tm_year + 1900, static_cast<unsigned>(parts.tm_mon + 1),
                               static_cast<unsigned>(parts.tm_mday));
}

void HabitsActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  selectedDay_ = today();
  unsigned shownDay = 0;
  habits::civilFromDays(selectedDay_, shownYear_, shownMonth_, shownDay);
  taskScroll_ = 0;
  clockValid_ = shownYear_ >= 2024 && shownYear_ <= 2100;
  if (!load()) view_ = View::Error;
  requestUpdate();
}

void HabitsActivity::onExit() {
  if (dirty_) save();
  Activity::onExit();
}

bool HabitsActivity::load() {
#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
  if (!Storage.ready()) {
    error_ = "SD CARD NOT READY";
    return false;
  }
  if (!Storage.ensureDirectoryExists(kRoot)) {
    error_ = "CANNOT CREATE HABITS FOLDER";
    return false;
  }
  habits::CsvFiles csv = habits::writeCsv(habits::Model{});
  std::string* targets[] = {&csv.habits, &csv.tasks, &csv.schedules, &csv.completions, &csv.exceptions};
  bool any = false;
  for (int i = 0; i < 5; ++i) {
    const std::string path = pathFor(kFiles[i]);
    if (!Storage.exists(path.c_str())) continue;
    any = true;
    const String contents = Storage.readFile(path.c_str());
    *targets[i] = contents.c_str();
  }
  if (!any) {
    dirty_ = true;
    return save();
  }
  const auto result = habits::readCsv(csv, model_);
  if (!result.ok) {
    char line[160];
    snprintf(line, sizeof(line), "%s line %d: %s", result.file.c_str(), result.line, result.message.c_str());
    error_ = line;
    return false;
  }
#endif
  return true;
}

bool HabitsActivity::save() {
#if defined(ARDUINO_ARCH_ESP32) || defined(SIMULATOR)
  const habits::CsvFiles csv = habits::writeCsv(model_);
  const std::string* values[] = {&csv.habits, &csv.tasks, &csv.schedules, &csv.completions, &csv.exceptions};
  for (int i = 0; i < 5; ++i) {
    const std::string temp = pathFor(kFiles[i], ".tmp");
    if (!Storage.writeFile(temp.c_str(), String(values[i]->c_str()))) {
      error_ = std::string("WRITE FAILED: ") + kFiles[i];
      return false;
    }
  }
  for (int i = 0; i < 5; ++i) {
    const std::string live = pathFor(kFiles[i]);
    const std::string backup = pathFor(kFiles[i], ".bak");
    const std::string temp = pathFor(kFiles[i], ".tmp");
    Storage.remove(backup.c_str());
    if (Storage.exists(live.c_str()) && !Storage.rename(live.c_str(), backup.c_str())) {
      error_ = std::string("BACKUP FAILED: ") + kFiles[i];
      return false;
    }
    if (!Storage.rename(temp.c_str(), live.c_str())) {
      if (Storage.exists(backup.c_str())) Storage.rename(backup.c_str(), live.c_str());
      error_ = std::string("REPLACE FAILED: ") + kFiles[i];
      return false;
    }
  }
#endif
  dirty_ = false;
  return true;
}

void HabitsActivity::markDirty(const bool immediate) {
  dirty_ = true;
  if (immediate && !save()) view_ = View::Error;
}

habits::Id HabitsActivity::selectedHabitId() const {
  return habitRow_ >= 0 && habitRow_ < static_cast<int>(model_.habits().size()) ? model_.habits()[habitRow_].id
                                                                               : habits::kInvalidId;
}

habits::Id HabitsActivity::selectedTaskId() const {
  const habits::Id habit = selectedHabitId();
  int row = 0;
  for (const auto& task : model_.tasks()) {
    if (task.habitId == habit && model_.isTaskActive(task.id, selectedDay_)) {
      if (row++ == taskRow_) return task.id;
    }
  }
  return habits::kInvalidId;
}

void HabitsActivity::clampSelections() {
  habitRow_ = std::max(0, std::min(habitRow_, static_cast<int>(model_.habits().size())));
  int count = 0;
  for (const auto& task : model_.tasks()) count += task.habitId == selectedHabitId() && model_.isTaskActive(task.id, selectedDay_);
  taskRow_ = std::max(0, std::min(taskRow_, count));
}

void HabitsActivity::editName(const bool task, const habits::Id id, const std::string& initial) {
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, task ? "TASK NAME" : "HABIT NAME",
                                                            initial, habits::kMaxNameBytes, InputType::Text);
  if (!keyboard) return;
  startActivityForResult(std::move(keyboard), [this, task, id](const ActivityResult& result) {
    if (result.isCancelled) return;
    const auto& entered = std::get<KeyboardResult>(result.data).text;
    if (entered.empty()) return;
    if (task) model_.renameTask(id, entered);
    else model_.renameHabit(id, entered);
    markDirty(true);
  });
}

void HabitsActivity::addHabit() {
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, "NEW HABIT", "",
                                                            habits::kMaxNameBytes, InputType::Text);
  if (!keyboard) return;
  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    if (result.isCancelled) return;
    const auto& name = std::get<KeyboardResult>(result.data).text;
    if (!name.empty()) {
      model_.addHabit(name, today());
      habitRow_ = static_cast<int>(model_.habits().size()) - 1;
      markDirty(true);
    }
  });
}

void HabitsActivity::addTask() {
  const habits::Id habit = selectedHabitId();
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, "NEW TASK", "",
                                                            habits::kMaxNameBytes, InputType::Text);
  if (!keyboard) return;
  startActivityForResult(std::move(keyboard), [this, habit](const ActivityResult& result) {
    if (result.isCancelled) return;
    const auto& name = std::get<KeyboardResult>(result.data).text;
    if (!name.empty()) {
      model_.addTask(habit, name, today());
      markDirty(true);
    }
  });
}

void HabitsActivity::showHabitMenu(const habits::Id id) {
  static const char* options[] = {"STATISTICS", "EDIT SCHEDULE", "RENAME", "ARCHIVE", "DELETE", "COPY", "MOVE UP", "MOVE DOWN"};
  popup_.show("HABIT OPTIONS", options, 8, 0, [this, id](const int choice) {
    const habits::Habit* habit = model_.findHabit(id);
    if (!habit) return;
    if (choice == 0) view_ = View::Stats;
    if (choice == 1) { editingMask_ = selectedScheduleMask(); scheduleRow_ = 0; view_ = View::Schedule; }
    if (choice == 2) editName(false, id, habit->name);
    if (choice == 3) { model_.setActive(id, today(), false); markDirty(true); clampSelections(); }
    if (choice == 4) { model_.deleteHabit(id); markDirty(true); clampSelections(); }
    if (choice == 5) { model_.copyHabit(id, today()); markDirty(true); }
    if (choice == 6) { model_.moveHabit(id, -1); --habitRow_; markDirty(true); clampSelections(); }
    if (choice == 7) { model_.moveHabit(id, 1); ++habitRow_; markDirty(true); clampSelections(); }
    requestUpdate();
  });
  requestUpdate();
}

void HabitsActivity::showTaskMenu(const habits::Id id) {
  static const char* options[] = {"RENAME", "DELETE", "MOVE UP", "MOVE DOWN"};
  popup_.show("TASK OPTIONS", options, 4, 0, [this, id](const int choice) {
    const habits::Task* task = model_.findTask(id);
    if (!task) return;
    if (choice == 0) editName(true, id, task->name);
    if (choice == 1) model_.retireTask(id, today());
    if (choice == 2) { model_.moveTask(id, -1); --taskRow_; }
    if (choice == 3) { model_.moveTask(id, 1); ++taskRow_; }
    markDirty(true);
    clampSelections();
    requestUpdate();
  });
  requestUpdate();
}

void HabitsActivity::showDateMenu(const int day) {
  const habits::Id id = selectedHabitId();
  const bool skipped = model_.isSkipped(id, day);
  const char* options[] = {skipped ? "UNSKIP DAY" : "SKIP DAY"};
  popup_.show("DATE OPTIONS", options, 1, 0, [this, id, day, skipped](int) {
    model_.setSkipped(id, day, !skipped);
    markDirty(true);
    requestUpdate();
  });
  requestUpdate();
}

uint8_t HabitsActivity::selectedScheduleMask() const {
  const auto id = selectedHabitId();
  uint8_t mask = habits::kAllWeekdays;
  int since = -2147483647;
  for (const auto& revision : model_.schedules()) {
    if (revision.habitId == id && revision.effectiveDay <= today() && revision.effectiveDay >= since) {
      mask = revision.weekdayMask;
      since = revision.effectiveDay;
    }
  }
  return mask;
}

void HabitsActivity::changeMonth(const int delta) {
  int month = static_cast<int>(shownMonth_) + delta;
  if (month < 1) { month = 12; --shownYear_; }
  if (month > 12) { month = 1; ++shownYear_; }
  const int maxFuture = today() + 366;
  if (habits::daysFromCivil(shownYear_, static_cast<unsigned>(month), 1) > maxFuture) return;
  shownMonth_ = static_cast<unsigned>(month);
  int y = 0; unsigned m = 0, d = 0;
  habits::civilFromDays(selectedDay_, y, m, d);
  selectedDay_ = habits::daysFromCivil(shownYear_, shownMonth_, std::min<unsigned>(d, daysInMonth(shownYear_, shownMonth_)));
  taskRow_ = taskScroll_ = 0;
}

void HabitsActivity::selectDateAt(const int x, const int y) {
  const int gridTop = kCalendarTop + kCalendarHeader + kWeekHeader;
  if (x < kCalendarLeft || x >= kCalendarLeft + kCalendarSide || y < gridTop || y >= kCalendarTop + kCalendarSide) return;
  const int cellW = kCalendarSide / 7;
  const int cellH = (kCalendarSide - kCalendarHeader - kWeekHeader) / 6;
  const int column = (x - kCalendarLeft) / cellW;
  const int row = (y - gridTop) / cellH;
  const int first = habits::daysFromCivil(shownYear_, shownMonth_, 1);
  const int day = 1 - habits::weekdayMonday0(first) + row * 7 + column;
  if (day >= 1 && day <= daysInMonth(shownYear_, shownMonth_)) {
    selectedDay_ = habits::daysFromCivil(shownYear_, shownMonth_, static_cast<unsigned>(day));
    taskRow_ = taskScroll_ = 0;
    requestUpdate();
  }
}

void HabitsActivity::loop() {
  if (popup_.isActive()) {
    popup_.handleInput(mappedInput, [this] { requestUpdate(); });
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (view_ == View::Habits || view_ == View::Error) shelf::leave(renderer, mappedInput);
    else { view_ = View::Habits; requestUpdate(); }
    return;
  }

  int hx = 0, hy = 0;
  if (mappedInput.isScreenTouchHeld(hx, hy)) {
    if (holdX_ < 0 || std::abs(hx - holdX_) > 12 || std::abs(hy - holdY_) > 12) {
      holdX_ = hx; holdY_ = hy; holdSince_ = millis(); holdFired_ = false;
    } else if (!holdFired_ && millis() - holdSince_ >= kHoldMs && surfaceRevealed()) {
      holdFired_ = true;
      mappedInput.swallowCurrentTouch();
      if (view_ == View::Habits) {
        const int row = (hy - kCalendarTop) / kRowH;
        if (row >= 0 && row < static_cast<int>(model_.habits().size())) { habitRow_ = row; showHabitMenu(selectedHabitId()); }
      } else if (view_ == View::Detail) {
        if (hy < kCalendarTop + kCalendarSide) showDateMenu(selectedDay_);
        else if (selectedDay_ == today() && selectedTaskId() != habits::kInvalidId) showTaskMenu(selectedTaskId());
      }
    }
    return;
  }
  holdX_ = holdY_ = -1;

  int x = 0, y = 0;
  if (mappedInput.wasScreenTapped(x, y) && surfaceRevealed()) {
    if (view_ == View::Habits) {
      const int row = (y - kCalendarTop) / kRowH;
      if (row >= 0 && row < static_cast<int>(model_.habits().size())) { habitRow_ = row; view_ = View::Detail; }
      else if (row == static_cast<int>(model_.habits().size())) addHabit();
      else if (row == static_cast<int>(model_.habits().size()) + 1) view_ = View::Stats;
      requestUpdate();
    } else if (view_ == View::Detail) {
      if (y < kCalendarTop + kCalendarHeader && x < 100) changeMonth(-1);
      else if (y < kCalendarTop + kCalendarHeader && x > renderer.getScreenWidth() - 100) changeMonth(1);
      else if (y < kCalendarTop + kCalendarSide) selectDateAt(x, y);
      else {
        const int row = taskScroll_ + (y - (kCalendarTop + kCalendarSide)) / kRowH;
        taskRow_ = row;
        const auto task = selectedTaskId();
        if (task == habits::kInvalidId) addTask();
        else if (selectedDay_ <= today() && !model_.isSkipped(selectedHabitId(), selectedDay_)) {
          model_.setTaskComplete(selectedHabitId(), task, selectedDay_,
                                 !model_.isTaskComplete(selectedHabitId(), task, selectedDay_));
          markDirty();
        }
        requestUpdate();
      }
    } else if (view_ == View::Schedule) {
      const int row = (y - kCalendarTop) / kRowH;
      if (row >= 0 && row < 7) { scheduleRow_ = row; editingMask_ ^= habits::weekdayBit(row); if (!editingMask_) editingMask_ |= habits::weekdayBit(row); }
      else if (row == 7) { model_.setSchedule(selectedHabitId(), today(), editingMask_); markDirty(true); view_ = View::Detail; }
      requestUpdate();
    }
    return;
  }

  const int direction = mappedInput.wasPressed(MappedInputManager::Button::NavPrevious) ? -1 :
                        mappedInput.wasPressed(MappedInputManager::Button::NavNext) ? 1 : 0;
  if (direction) {
    if (view_ == View::Habits) habitRow_ += direction;
    else if (view_ == View::Detail) taskRow_ += direction;
    else if (view_ == View::Schedule) scheduleRow_ = std::max(0, std::min(7, scheduleRow_ + direction));
    clampSelections();
    requestUpdate();
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (view_ == View::Habits) {
      if (habitRow_ == static_cast<int>(model_.habits().size())) addHabit(); else view_ = View::Detail;
    } else if (view_ == View::Detail) {
      const auto task = selectedTaskId();
      if (task == habits::kInvalidId) addTask();
      else if (selectedDay_ <= today() && !model_.isSkipped(selectedHabitId(), selectedDay_)) {
        model_.setTaskComplete(selectedHabitId(), task, selectedDay_, !model_.isTaskComplete(selectedHabitId(), task, selectedDay_));
        markDirty();
      }
    } else if (view_ == View::Schedule) {
      if (scheduleRow_ < 7) { editingMask_ ^= habits::weekdayBit(scheduleRow_); if (!editingMask_) editingMask_ |= habits::weekdayBit(scheduleRow_); }
      else { model_.setSchedule(selectedHabitId(), today(), editingMask_); markDirty(true); view_ = View::Detail; }
    }
    requestUpdate();
  }
}

uint32_t HabitsActivity::surfaceMeaning() const {
  uint32_t meaning = paintclock::mixMeaning(paintclock::kMeaningSeed, static_cast<uint32_t>(view_));
  meaning = paintclock::mixMeaning(meaning, static_cast<uint32_t>(habitRow_ + 1));
  meaning = paintclock::mixMeaning(meaning, static_cast<uint32_t>(selectedDay_));
  return meaning;
}

void HabitsActivity::drawTextFit(const int font, const int x, const int y, const int width, const std::string& text,
                                 const bool ink) const {
  if (renderer.getTextWidth(font, text.c_str()) <= width) { renderer.drawText(font, x, y, text.c_str(), ink); return; }
  std::string shown = text;
  while (!shown.empty() && renderer.getTextWidth(font, (shown + "...").c_str()) > width) shown.pop_back();
  shown += "...";
  renderer.drawText(font, x, y, shown.c_str(), ink);
}

void HabitsActivity::drawDayMark(const int x, const int y, const int size, const habits::DayState state) const {
  const int stroke = 2;
  if (state == habits::DayState::NotApplicable || state == habits::DayState::Future) return;
  if (state == habits::DayState::Skipped) {
    renderer.drawLine(x, y, x + size, y + size, true);
    renderer.drawLine(x + size, y, x, y + size, true);
    return;
  }
  renderer.drawRoundedRect(x, y, size, size, stroke, size / 2, true);
  if (state == habits::DayState::Complete) renderer.fillRoundedRect(x + 5, y + 5, size - 10, size - 10, (size - 10) / 2, Black);
  if (state == habits::DayState::Partial) renderer.fillRect(x + 2, y + size / 2, size - 4, size / 2 - 2, true);
}

void HabitsActivity::drawHabits() {
  const int width = renderer.getScreenWidth();
  int y = kCalendarTop;
  for (int i = 0; i < static_cast<int>(model_.habits().size()); ++i, y += kRowH) {
    const auto& habit = model_.habits()[i];
    if (i == habitRow_) renderer.fillRect(10, y, width - 20, kRowH - 2, true);
    drawTextFit(toybox::kUiFontId, 24, y + 11, width - 100, habit.name, i != habitRow_);
    drawDayMark(width - 58, y + 12, 28, model_.dayState(habit.id, today(), today()));
    renderer.fillRect(12, y + kRowH - 2, width - 24, 1, true);
  }
  const char* extras[] = {"+ ADD HABIT", "STATISTICS"};
  for (int i = 0; i < 2; ++i, y += kRowH) {
    const bool selected = habitRow_ == static_cast<int>(model_.habits().size()) + i;
    if (selected) renderer.fillRect(10, y, width - 20, kRowH - 2, true);
    renderer.drawText(toybox::kUiFontId, 24, y + 11, extras[i], !selected);
  }
}

void HabitsActivity::drawDetail() {
  const auto habitId = selectedHabitId();
  const int cellW = kCalendarSide / 7;
  const int cellH = (kCalendarSide - kCalendarHeader - kWeekHeader) / 6;
  char title[40];
  snprintf(title, sizeof(title), "<  %s %d  >", monthName(shownMonth_), shownYear_);
  renderer.drawText(toybox::kUiFontId, (renderer.getScreenWidth() - renderer.getTextWidth(toybox::kUiFontId, title)) / 2,
                    kCalendarTop + 12, title, true);
  static const char* days[] = {"M", "T", "W", "T", "F", "S", "S"};
  for (int column = 0; column < 7; ++column) renderer.drawText(toybox::kTileFontId, kCalendarLeft + column * cellW + 24, kCalendarTop + kCalendarHeader + 4, days[column], true);
  const int first = habits::daysFromCivil(shownYear_, shownMonth_, 1);
  const int start = 1 - habits::weekdayMonday0(first);
  const int gridTop = kCalendarTop + kCalendarHeader + kWeekHeader;
  for (int row = 0; row < 6; ++row) {
    for (int column = 0; column < 7; ++column) {
      const int value = start + row * 7 + column;
      const int x = kCalendarLeft + column * cellW;
      const int y = gridTop + row * cellH;
      renderer.drawRect(x, y, cellW, cellH, value >= 1 && value <= daysInMonth(shownYear_, shownMonth_) &&
                        habits::daysFromCivil(shownYear_, shownMonth_, value) == selectedDay_ ? 3 : 1, true);
      if (value < 1 || value > daysInMonth(shownYear_, shownMonth_)) continue;
      char number[4]; snprintf(number, sizeof(number), "%d", value);
      renderer.drawText(toybox::kTileFontId, x + cellW - renderer.getTextWidth(toybox::kTileFontId, number) - 4, y + 2, number, true);
      const int day = habits::daysFromCivil(shownYear_, shownMonth_, static_cast<unsigned>(value));
      drawDayMark(x + (cellW - 25) / 2, y + 23, 25, model_.dayState(habitId, day, today()));
    }
  }
  int y = kCalendarTop + kCalendarSide;
  int row = 0;
  for (const auto& task : model_.tasks()) {
    if (task.habitId != habitId || !model_.isTaskActive(task.id, selectedDay_)) continue;
    if (row++ < taskScroll_) continue;
    if (y + kRowH > renderer.getScreenHeight() - 38) break;
    renderer.drawRect(18, y + 14, 25, 25, 2, true);
    if (model_.isTaskComplete(habitId, task.id, selectedDay_)) {
      renderer.drawLine(21, y + 27, 29, y + 35, true); renderer.drawLine(29, y + 35, 40, y + 17, true);
    }
    drawTextFit(toybox::kUiFontId, 56, y + 10, renderer.getScreenWidth() - 70, task.name);
    y += kRowH;
  }
  if (y + kRowH <= renderer.getScreenHeight() - 38) renderer.drawText(toybox::kUiFontId, 24, y + 10, "+ ADD TASK", true);
}

void HabitsActivity::drawSchedule() {
  static const char* names[] = {"MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY", "SUNDAY"};
  int y = kCalendarTop;
  for (int i = 0; i < 7; ++i, y += kRowH) {
    if (scheduleRow_ == i) renderer.fillRect(10, y, renderer.getScreenWidth() - 20, kRowH - 2, true);
    renderer.drawRect(20, y + 13, 25, 25, 2, scheduleRow_ != i);
    if (editingMask_ & habits::weekdayBit(i)) { renderer.drawLine(23, y + 27, 31, y + 35, scheduleRow_ != i); renderer.drawLine(31, y + 35, 42, y + 17, scheduleRow_ != i); }
    renderer.drawText(toybox::kUiFontId, 58, y + 10, names[i], scheduleRow_ != i);
  }
  if (scheduleRow_ == 7) renderer.fillRect(10, y, renderer.getScreenWidth() - 20, kRowH - 2, true);
  renderer.drawText(toybox::kUiFontId, 24, y + 10, "SAVE SCHEDULE", scheduleRow_ != 7);
}

void HabitsActivity::drawStats() {
  const auto stats = habits::summarize(model_, selectedHabitId(), today() - 364, today(), today());
  char line[96];
  int y = kCalendarTop + 20;
  auto draw = [&](const char* label, const int value) {
    snprintf(line, sizeof(line), "%s  %d", label, value); renderer.drawText(toybox::kUiFontId, 28, y, line, true); y += 58;
  };
  draw("COMPLETE DAYS", stats.completeDays); draw("PARTIAL DAYS", stats.partialDays); draw("MISSED DAYS", stats.missedDays);
  draw("SKIPPED DAYS", stats.skippedDays); draw("COMPLETE DAY %", stats.completeDayPercent()); draw("TASK %", stats.taskPercent());
  y += 18;
  renderer.drawText(toybox::kTileFontId, 28, y, "LAST 12 MONTHS / CURRENT MONTH PARTIAL", true);
}

void HabitsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const int width = renderer.getScreenWidth();
  const char* heading = view_ == View::Detail && model_.findHabit(selectedHabitId()) ? model_.findHabit(selectedHabitId())->name.c_str() :
                        view_ == View::Schedule ? "SCHEDULE" : view_ == View::Stats ? "STATISTICS" : view_ == View::Error ? "HABITS ERROR" : "HABITS";
  renderer.fillRect(0, 0, width, 62, true);
  drawTextFit(toybox::kDisplayFontId, 18, 11, width - 36, heading, false);
  if (view_ == View::Habits) drawHabits();
  else if (view_ == View::Detail) drawDetail();
  else if (view_ == View::Schedule) drawSchedule();
  else if (view_ == View::Stats) drawStats();
  else UITheme::drawCenteredWrappedText(renderer, Rect{20, 90, width - 40, renderer.getScreenHeight() - 150}, toybox::kUiFontId, error_.c_str(), 5);
  const auto labels = mappedInput.mapLabels("Back", "Select", "Up", "Down");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  if (popup_.processRender(renderer, mappedInput)) return;
  renderer.displayBuffer();
}
