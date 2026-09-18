#include "HabitModel.h"

#include <algorithm>

namespace habits {
namespace {
int floorMod(const int value, const int modulus) {
  const int remainder = value % modulus;
  return remainder < 0 ? remainder + modulus : remainder;
}

bool validName(const std::string& name) { return !name.empty(); }
}  // namespace

int daysFromCivil(int year, const unsigned month, const unsigned day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yearOfEra = static_cast<unsigned>(year - era * 400);
  const unsigned dayOfYear = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
  return era * 146097 + static_cast<int>(dayOfEra) - 719468;
}

void civilFromDays(const int value, int& year, unsigned& month, unsigned& day) {
  const int shifted = value + 719468;
  const int era = (shifted >= 0 ? shifted : shifted - 146096) / 146097;
  const unsigned dayOfEra = static_cast<unsigned>(shifted - era * 146097);
  const unsigned yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
  year = static_cast<int>(yearOfEra) + era * 400;
  const unsigned dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
  const unsigned monthPrime = (5 * dayOfYear + 2) / 153;
  day = dayOfYear - (153 * monthPrime + 2) / 5 + 1;
  month = monthPrime + (monthPrime < 10 ? 3 : -9);
  year += month <= 2;
}

int weekdayMonday0(const int day) { return floorMod(day + 3, 7); }

std::string truncateUtf8(const std::string& text, const size_t maxBytes) {
  if (text.size() <= maxBytes) return text;
  size_t end = maxBytes;
  while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xc0u) == 0x80u) --end;
  return text.substr(0, end);
}

const Habit* Model::findHabit(const Id id) const {
  const auto found = std::find_if(habits_.begin(), habits_.end(), [id](const Habit& habit) { return habit.id == id; });
  return found == habits_.end() ? nullptr : &*found;
}

const Task* Model::findTask(const Id id) const {
  const auto found = std::find_if(tasks_.begin(), tasks_.end(), [id](const Task& task) { return task.id == id; });
  return found == tasks_.end() ? nullptr : &*found;
}

Id Model::addHabit(const std::string& name, const int createdDay) {
  if (!validName(name) || habits_.size() >= kMaxHabits || tasks_.size() >= kMaxTasks) return kInvalidId;
  const Id id = nextId_++;
  habits_.push_back({id, truncateUtf8(name), createdDay, static_cast<int>(habits_.size())});
  schedules_.push_back({id, createdDay, kAllWeekdays});
  active_.push_back({id, createdDay, true});
  if (addTask(id, name, createdDay) == kInvalidId) {
    habits_.pop_back();
    schedules_.pop_back();
    active_.pop_back();
    return kInvalidId;
  }
  return id;
}

Id Model::addTask(const Id habitId, const std::string& name, const int createdDay) {
  if (!findHabit(habitId) || !validName(name) || tasks_.size() >= kMaxTasks) return kInvalidId;
  size_t count = 0;
  for (const auto& task : tasks_) count += task.habitId == habitId && task.retiredDay == 0;
  if (count >= kMaxTasksPerHabit) return kInvalidId;
  const Id id = nextId_++;
  tasks_.push_back({id, habitId, truncateUtf8(name), createdDay, 0, static_cast<int>(count)});
  return id;
}

bool Model::renameHabit(const Id habitId, const std::string& name) {
  if (!validName(name)) return false;
  for (auto& habit : habits_) {
    if (habit.id == habitId) {
      habit.name = truncateUtf8(name);
      return true;
    }
  }
  return false;
}

bool Model::renameTask(const Id taskId, const std::string& name) {
  if (!validName(name)) return false;
  for (auto& task : tasks_) {
    if (task.id == taskId) {
      task.name = truncateUtf8(name);
      return true;
    }
  }
  return false;
}

bool Model::retireTask(const Id taskId, const int day) {
  for (auto& task : tasks_) {
    if (task.id == taskId && day >= task.createdDay) {
      task.retiredDay = day;
      return true;
    }
  }
  return false;
}

bool Model::setSchedule(const Id habitId, const int effectiveDay, const uint8_t weekdayMask) {
  if (!findHabit(habitId) || (weekdayMask & kAllWeekdays) == 0) return false;
  schedules_.push_back({habitId, effectiveDay, static_cast<uint8_t>(weekdayMask & kAllWeekdays)});
  return true;
}

bool Model::setActive(const Id habitId, const int effectiveDay, const bool active) {
  if (!findHabit(habitId)) return false;
  active_.push_back({habitId, effectiveDay, active});
  return true;
}

bool Model::setTaskComplete(const Id habitId, const Id taskId, const int day, const bool complete) {
  const Task* task = findTask(taskId);
  if (!task || task->habitId != habitId || !isTaskActive(taskId, day) || !isApplicable(habitId, day) ||
      isSkipped(habitId, day)) {
    return false;
  }
  for (auto& value : completions_) {
    if (value.habitId == habitId && value.taskId == taskId && value.day == day) {
      value.complete = complete;
      return true;
    }
  }
  completions_.push_back({habitId, taskId, day, complete});
  return true;
}

bool Model::setSkipped(const Id habitId, const int day, const bool skipped) {
  if (!isApplicable(habitId, day)) return false;
  for (auto& value : exceptions_) {
    if (value.habitId == habitId && value.day == day) {
      value.skipped = skipped;
      return true;
    }
  }
  exceptions_.push_back({habitId, day, skipped});
  return true;
}

bool Model::deleteHabit(const Id habitId) {
  const auto before = habits_.size();
  habits_.erase(std::remove_if(habits_.begin(), habits_.end(), [habitId](const Habit& v) { return v.id == habitId; }),
                habits_.end());
  if (habits_.size() == before) return false;
  tasks_.erase(std::remove_if(tasks_.begin(), tasks_.end(), [habitId](const Task& v) { return v.habitId == habitId; }), tasks_.end());
  schedules_.erase(std::remove_if(schedules_.begin(), schedules_.end(), [habitId](const ScheduleRevision& v) { return v.habitId == habitId; }), schedules_.end());
  active_.erase(std::remove_if(active_.begin(), active_.end(), [habitId](const ActiveRevision& v) { return v.habitId == habitId; }), active_.end());
  completions_.erase(std::remove_if(completions_.begin(), completions_.end(), [habitId](const Completion& v) { return v.habitId == habitId; }), completions_.end());
  exceptions_.erase(std::remove_if(exceptions_.begin(), exceptions_.end(), [habitId](const Exception& v) { return v.habitId == habitId; }), exceptions_.end());
  return true;
}

bool Model::deleteTask(const Id taskId) {
  const Task* task = findTask(taskId);
  if (!task) return false;
  int remaining = 0;
  for (const auto& value : tasks_) remaining += value.habitId == task->habitId && value.id != taskId && value.retiredDay == 0;
  if (remaining == 0) return false;
  tasks_.erase(std::remove_if(tasks_.begin(), tasks_.end(), [taskId](const Task& v) { return v.id == taskId; }), tasks_.end());
  completions_.erase(std::remove_if(completions_.begin(), completions_.end(), [taskId](const Completion& v) { return v.taskId == taskId; }), completions_.end());
  return true;
}

Id Model::copyHabit(const Id habitId, const int createdDay) {
  const Habit* original = findHabit(habitId);
  if (!original) return kInvalidId;
  const std::string copiedName = original->name + " Copy";
  std::vector<std::string> taskNames;
  for (const auto& task : tasks_) {
    if (task.habitId == habitId && isTaskActive(task.id, createdDay)) taskNames.push_back(task.name);
  }
  const Id copied = addHabit(copiedName, createdDay);
  if (copied == kInvalidId) return copied;
  bool replacedFirst = false;
  for (const auto& taskName : taskNames) {
    if (!replacedFirst) {
      renameTask(tasks_.back().id, taskName);
      replacedFirst = true;
    } else {
      addTask(copied, taskName, createdDay);
    }
  }
  uint8_t mask = kAllWeekdays;
  int since = -2147483647;
  for (const auto& revision : schedules_) {
    if (revision.habitId == habitId && revision.effectiveDay <= createdDay && revision.effectiveDay >= since) {
      mask = revision.weekdayMask;
      since = revision.effectiveDay;
    }
  }
  setSchedule(copied, createdDay, mask);
  return copied;
}

bool Model::moveHabit(const Id habitId, const int delta) {
  auto found = std::find_if(habits_.begin(), habits_.end(), [habitId](const Habit& value) { return value.id == habitId; });
  if (found == habits_.end()) return false;
  const auto index = static_cast<int>(found - habits_.begin());
  const int target = index + delta;
  if (target < 0 || target >= static_cast<int>(habits_.size())) return false;
  std::iter_swap(habits_.begin() + index, habits_.begin() + target);
  for (size_t i = 0; i < habits_.size(); ++i) habits_[i].order = static_cast<int>(i);
  return true;
}

bool Model::moveTask(const Id taskId, const int delta) {
  auto found = std::find_if(tasks_.begin(), tasks_.end(), [taskId](const Task& value) { return value.id == taskId; });
  if (found == tasks_.end()) return false;
  std::vector<size_t> siblings;
  for (size_t i = 0; i < tasks_.size(); ++i) if (tasks_[i].habitId == found->habitId && tasks_[i].retiredDay == 0) siblings.push_back(i);
  auto position = std::find(siblings.begin(), siblings.end(), static_cast<size_t>(found - tasks_.begin()));
  const int index = static_cast<int>(position - siblings.begin());
  const int target = index + delta;
  if (target < 0 || target >= static_cast<int>(siblings.size())) return false;
  std::iter_swap(tasks_.begin() + siblings[index], tasks_.begin() + siblings[target]);
  for (size_t i = 0; i < siblings.size(); ++i) tasks_[siblings[i]].order = static_cast<int>(i);
  return true;
}

void Model::clear() {
  nextId_ = 1;
  habits_.clear();
  tasks_.clear();
  schedules_.clear();
  active_.clear();
  completions_.clear();
  exceptions_.clear();
}

bool Model::restoreHabit(const Habit& habit) {
  if (habit.id == kInvalidId || !validName(habit.name) || habits_.size() >= kMaxHabits || findHabit(habit.id)) return false;
  Habit clean = habit;
  clean.name = truncateUtf8(clean.name);
  habits_.push_back(clean);
  nextId_ = std::max(nextId_, habit.id + 1);
  return true;
}

bool Model::restoreTask(const Task& task) {
  if (task.id == kInvalidId || !findHabit(task.habitId) || !validName(task.name) || tasks_.size() >= kMaxTasks ||
      findTask(task.id)) {
    return false;
  }
  size_t count = 0;
  for (const auto& existing : tasks_) count += existing.habitId == task.habitId;
  if (count >= kMaxTasksPerHabit) return false;
  Task clean = task;
  clean.name = truncateUtf8(clean.name);
  tasks_.push_back(clean);
  nextId_ = std::max(nextId_, task.id + 1);
  return true;
}

bool Model::restoreSchedule(const ScheduleRevision& revision) {
  if (!findHabit(revision.habitId) || (revision.weekdayMask & kAllWeekdays) == 0) return false;
  schedules_.push_back(revision);
  return true;
}

bool Model::restoreActive(const ActiveRevision& revision) {
  if (!findHabit(revision.habitId)) return false;
  active_.push_back(revision);
  return true;
}

bool Model::restoreCompletion(const Completion& completion) {
  const Task* task = findTask(completion.taskId);
  if (!task || task->habitId != completion.habitId) return false;
  completions_.push_back(completion);
  return true;
}

bool Model::restoreException(const Exception& exception) {
  if (!findHabit(exception.habitId)) return false;
  exceptions_.push_back(exception);
  return true;
}

bool Model::isApplicable(const Id habitId, const int day) const {
  const Habit* habit = findHabit(habitId);
  if (!habit || day < habit->createdDay) return false;
  bool active = false;
  int activeSince = -2147483647;
  for (const auto& revision : active_) {
    if (revision.habitId == habitId && revision.effectiveDay <= day && revision.effectiveDay >= activeSince) {
      active = revision.active;
      activeSince = revision.effectiveDay;
    }
  }
  if (!active) return false;
  uint8_t mask = kAllWeekdays;
  int scheduleSince = -2147483647;
  for (const auto& revision : schedules_) {
    if (revision.habitId == habitId && revision.effectiveDay <= day && revision.effectiveDay >= scheduleSince) {
      mask = revision.weekdayMask;
      scheduleSince = revision.effectiveDay;
    }
  }
  return (mask & weekdayBit(weekdayMonday0(day))) != 0;
}

bool Model::isTaskActive(const Id taskId, const int day) const {
  const Task* task = findTask(taskId);
  return task && day >= task->createdDay && (task->retiredDay == 0 || day < task->retiredDay);
}

bool Model::isTaskComplete(const Id habitId, const Id taskId, const int day) const {
  for (auto value = completions_.rbegin(); value != completions_.rend(); ++value) {
    if (value->habitId == habitId && value->taskId == taskId && value->day == day) return value->complete;
  }
  return false;
}

bool Model::isSkipped(const Id habitId, const int day) const {
  for (auto value = exceptions_.rbegin(); value != exceptions_.rend(); ++value) {
    if (value->habitId == habitId && value->day == day) return value->skipped;
  }
  return false;
}

DayState Model::dayState(const Id habitId, const int day, const int today) const {
  if (!isApplicable(habitId, day)) return DayState::NotApplicable;
  if (day > today) return DayState::Future;
  if (isSkipped(habitId, day)) return DayState::Skipped;
  int total = 0;
  int complete = 0;
  for (const auto& task : tasks_) {
    if (task.habitId == habitId && isTaskActive(task.id, day)) {
      ++total;
      complete += isTaskComplete(habitId, task.id, day);
    }
  }
  if (total == 0 || complete == 0) return DayState::Empty;
  return complete == total ? DayState::Complete : DayState::Partial;
}

}  // namespace habits
