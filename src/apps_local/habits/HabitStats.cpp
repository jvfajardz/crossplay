#include "HabitStats.h"

namespace habits {

int Stats::completeDayPercent() const {
  const int denominator = completeDays + partialDays + missedDays;
  return denominator == 0 ? -1 : completeDays * 100 / denominator;
}

int Stats::taskPercent() const { return taskInstances == 0 ? -1 : completedTasks * 100 / taskInstances; }

Stats summarize(const Model& model, const Id habitId, const int firstDay, const int lastDay, const int today) {
  Stats result;
  for (int day = firstDay; day <= lastDay; ++day) {
    const DayState state = model.dayState(habitId, day, today);
    if (state == DayState::Skipped) {
      ++result.skippedDays;
      continue;
    }
    if (state == DayState::NotApplicable || state == DayState::Future) continue;
    if (day < today) {
      const int weekday = weekdayMonday0(day);
      ++result.byWeekdayEligible[weekday];
      if (state == DayState::Complete) {
        ++result.completeDays;
        ++result.byWeekdayComplete[weekday];
      } else if (state == DayState::Partial) {
        ++result.partialDays;
      } else {
        ++result.missedDays;
      }
    }
    for (const auto& task : model.tasks()) {
      if (task.habitId == habitId && model.isTaskActive(task.id, day)) {
        ++result.taskInstances;
        result.completedTasks += model.isTaskComplete(habitId, task.id, day);
      }
    }
  }
  return result;
}

}  // namespace habits
