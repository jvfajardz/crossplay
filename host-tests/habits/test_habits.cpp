#include <cstdio>
#include <string>

#include "../../src/apps_local/habits/HabitCsvStore.h"
#include "../../src/apps_local/habits/HabitModel.h"
#include "../../src/apps_local/habits/HabitStats.h"

namespace {
int checks = 0;
int failures = 0;

void check(bool ok, const char* message) {
  ++checks;
  if (!ok) {
    ++failures;
    std::printf("  FAIL: %s\n", message);
  }
}

void testCreationAndNames() {
  habits::Model model;
  const int day = habits::daysFromCivil(2026, 9, 18);
  const auto id = model.addHabit("Drink Water", day);
  check(id != habits::kInvalidId, "habit is created");
  check(model.habits().size() == 1, "one habit exists");
  check(model.tasks().size() == 1, "first task is created automatically");
  check(model.tasks()[0].name == "Drink Water", "first task copies the habit name");
  check(model.renameHabit(id, "Hydrate"), "habit can be renamed");
  check(model.tasks()[0].name == "Drink Water", "task name remains independent");

  const std::string longUtf8 = "123456789012345678901234567890123456789Ã©Z";
  check(habits::truncateUtf8(longUtf8, 40).size() == 40, "name is capped at 40 bytes");
  check(habits::truncateUtf8(longUtf8, 40).back() != static_cast<char>(0xc3),
        "UTF-8 truncation never leaves a partial code point");
}

void testScheduleAndStates() {
  habits::Model model;
  const int monday = habits::daysFromCivil(2026, 9, 14);
  const auto id = model.addHabit("Gym", monday);
  check(model.setSchedule(id, monday, habits::weekdayBit(0) | habits::weekdayBit(2) | habits::weekdayBit(4)),
        "weekday schedule is accepted");
  check(model.dayState(id, monday, monday) == habits::DayState::Empty, "scheduled day starts empty");
  check(model.dayState(id, monday + 1, monday + 1) == habits::DayState::NotApplicable, "unscheduled weekday is blank");
  const auto second = model.addTask(id, "Pack bag", monday);
  check(second != habits::kInvalidId, "second task is created");
  check(model.setTaskComplete(id, model.tasks()[0].id, monday, true), "first task can be checked");
  check(model.dayState(id, monday, monday) == habits::DayState::Partial, "some tasks checked is partial");
  check(model.setTaskComplete(id, second, monday, true), "second task can be checked");
  check(model.dayState(id, monday, monday) == habits::DayState::Complete, "all tasks checked is complete");
  check(model.setSkipped(id, monday, true), "day can be skipped");
  check(model.dayState(id, monday, monday) == habits::DayState::Skipped, "skip overrides visible completion");
  check(model.setSkipped(id, monday, false), "day can be unskipped");
  check(model.dayState(id, monday, monday) == habits::DayState::Complete, "unskip restores underlying completion");
  check(model.dayState(id, monday + 14, monday) == habits::DayState::Future, "future applicable day has no symbol");
}

void testHistoricalScheduleAndStats() {
  habits::Model model;
  const int monday = habits::daysFromCivil(2026, 9, 7);
  const auto id = model.addHabit("Read", monday);
  model.setSchedule(id, monday, habits::kAllWeekdays);
  model.setTaskComplete(id, model.tasks()[0].id, monday, true);
  model.setSkipped(id, monday + 1, true);
  model.setSchedule(id, monday + 7, habits::weekdayBit(0));

  check(model.scheduleMask(id, monday + 2) == habits::kAllWeekdays, "historical schedule query returns the old mask");
  check(model.scheduleMask(id, monday + 9) == habits::weekdayBit(0), "historical schedule query returns the new mask");
  check(model.isActive(id, monday + 9), "habit is active before archive");
  model.setActive(id, monday + 10, false);
  check(!model.isActive(id, monday + 10), "habit is inactive from its archive date");
  model.setActive(id, monday + 11, true);
  check(model.isActive(id, monday + 11), "archived habit can be restored");

  check(model.isApplicable(id, monday + 2), "old schedule remains effective historically");
  check(!model.isApplicable(id, monday + 9), "new schedule governs later history");

  const auto stats = habits::summarize(model, id, monday, monday + 2, monday + 2);
  check(stats.completeDays == 1, "stats count complete days");
  check(stats.skippedDays == 1, "stats count skipped separately");
  check(stats.missedDays == 0, "current day is excluded from missed denominator");
  check(stats.completeDayPercent() == 100, "skipped and current days are excluded from completion rate");
}

void testCsvRoundTrip() {
  habits::Model source;
  const int day = habits::daysFromCivil(2026, 9, 18);
  const auto id = source.addHabit("Read, \"deeply\"", day);
  source.addTask(id, "Chapter 2", day);
  source.setSchedule(id, day, habits::weekdayBit(0) | habits::weekdayBit(4));
  source.setActive(id, day + 10, false);
  source.setTaskComplete(id, source.tasks()[0].id, day, true);
  source.setSkipped(id, day + 7, true);
  const habits::CsvFiles csv = habits::writeCsv(source);
  habits::Model loaded;
  const auto result = habits::readCsv(csv, loaded);
  check(result.ok, "five CSV files parse after writing");
  check(loaded.habits().size() == 1 && loaded.habits()[0].name == "Read, \"deeply\"",
        "quoted CSV habit name round-trips");
  check(loaded.tasks().size() == 2, "tasks round-trip");
  check(loaded.dayState(id, day, day) == habits::DayState::Partial, "completion round-trips");
  check(!loaded.isApplicable(id, day + 14), "archive revision round-trips");
}
}  // namespace

int main() {
  std::printf("Habits\n");
  testCreationAndNames();
  testScheduleAndStates();
  testHistoricalScheduleAndStats();
  testCsvRoundTrip();
  std::printf("%s %d checks, %d failed\n", failures == 0 ? "PASS" : "FAIL", checks, failures);
  return failures == 0 ? 0 : 1;
}
