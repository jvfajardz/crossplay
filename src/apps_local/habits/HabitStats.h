#pragma once

#include "HabitModel.h"

namespace habits {

struct Stats {
  int completeDays = 0;
  int partialDays = 0;
  int missedDays = 0;
  int skippedDays = 0;
  int completedTasks = 0;
  int taskInstances = 0;
  int byWeekdayComplete[7] = {};
  int byWeekdayEligible[7] = {};

  int completeDayPercent() const;
  int taskPercent() const;
};

Stats summarize(const Model& model, Id habitId, int firstDay, int lastDay, int today);

}  // namespace habits
