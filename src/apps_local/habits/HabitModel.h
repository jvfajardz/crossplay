#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace habits {

using Id = uint32_t;
constexpr Id kInvalidId = 0;
constexpr uint8_t kAllWeekdays = 0x7f;
constexpr size_t kMaxNameBytes = 40;
constexpr size_t kMaxHabits = 64;
constexpr size_t kMaxTasksPerHabit = 64;
constexpr size_t kMaxTasks = 512;

enum class DayState : uint8_t { NotApplicable, Future, Empty, Partial, Complete, Skipped };

struct Habit {
  Id id = kInvalidId;
  std::string name;
  int createdDay = 0;
  int order = 0;
};

struct Task {
  Id id = kInvalidId;
  Id habitId = kInvalidId;
  std::string name;
  int createdDay = 0;
  int retiredDay = 0;
  int order = 0;
};

struct ScheduleRevision {
  Id habitId = kInvalidId;
  int effectiveDay = 0;
  uint8_t weekdayMask = kAllWeekdays;
};

struct ActiveRevision {
  Id habitId = kInvalidId;
  int effectiveDay = 0;
  bool active = true;
};

struct Completion {
  Id habitId = kInvalidId;
  Id taskId = kInvalidId;
  int day = 0;
  bool complete = false;
};

struct Exception {
  Id habitId = kInvalidId;
  int day = 0;
  bool skipped = false;
};

int daysFromCivil(int year, unsigned month, unsigned day);
void civilFromDays(int value, int& year, unsigned& month, unsigned& day);
int weekdayMonday0(int day);
constexpr uint8_t weekdayBit(int mondayZero) { return static_cast<uint8_t>(1u << mondayZero); }
std::string truncateUtf8(const std::string& text, size_t maxBytes = kMaxNameBytes);

class Model {
 public:
  Id addHabit(const std::string& name, int createdDay);
  Id addTask(Id habitId, const std::string& name, int createdDay);
  bool renameHabit(Id habitId, const std::string& name);
  bool renameTask(Id taskId, const std::string& name);
  bool retireTask(Id taskId, int day);
  bool setSchedule(Id habitId, int effectiveDay, uint8_t weekdayMask);
  bool setActive(Id habitId, int effectiveDay, bool active);
  bool setTaskComplete(Id habitId, Id taskId, int day, bool complete);
  bool setSkipped(Id habitId, int day, bool skipped);
  bool deleteHabit(Id habitId);
  bool deleteTask(Id taskId);
  Id copyHabit(Id habitId, int createdDay);
  bool moveHabit(Id habitId, int delta);
  bool moveTask(Id taskId, int delta);

  void clear();
  bool restoreHabit(const Habit& habit);
  bool restoreTask(const Task& task);
  bool restoreSchedule(const ScheduleRevision& revision);
  bool restoreActive(const ActiveRevision& revision);
  bool restoreCompletion(const Completion& completion);
  bool restoreException(const Exception& exception);

  bool isApplicable(Id habitId, int day) const;
  bool isTaskActive(Id taskId, int day) const;
  bool isTaskComplete(Id habitId, Id taskId, int day) const;
  bool isSkipped(Id habitId, int day) const;
  DayState dayState(Id habitId, int day, int today) const;

  const Habit* findHabit(Id id) const;
  const Task* findTask(Id id) const;
  const std::vector<Habit>& habits() const { return habits_; }
  const std::vector<Task>& tasks() const { return tasks_; }
  const std::vector<ScheduleRevision>& schedules() const { return schedules_; }
  const std::vector<ActiveRevision>& activeRevisions() const { return active_; }
  const std::vector<Completion>& completions() const { return completions_; }
  const std::vector<Exception>& exceptions() const { return exceptions_; }

 private:
  Id nextId_ = 1;
  std::vector<Habit> habits_;
  std::vector<Task> tasks_;
  std::vector<ScheduleRevision> schedules_;
  std::vector<ActiveRevision> active_;
  std::vector<Completion> completions_;
  std::vector<Exception> exceptions_;
};

}  // namespace habits
