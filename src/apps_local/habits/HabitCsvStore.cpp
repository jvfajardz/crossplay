#include "HabitCsvStore.h"

#include <cerrno>
#include <cstdlib>
#include <sstream>
#include <vector>

namespace habits {
namespace {
std::string quote(const std::string& value) {
  std::string out = "\"";
  for (const char ch : value) {
    out += ch;
    if (ch == '"') out += '"';
  }
  out += '"';
  return out;
}

bool split(const std::string& line, std::vector<std::string>& fields) {
  fields.clear();
  std::string field;
  bool quoted = false;
  for (size_t i = 0; i < line.size(); ++i) {
    const char ch = line[i];
    if (quoted) {
      if (ch == '"') {
        if (i + 1 < line.size() && line[i + 1] == '"') {
          field += '"';
          ++i;
        } else {
          quoted = false;
        }
      } else {
        field += ch;
      }
    } else if (ch == '"' && field.empty()) {
      quoted = true;
    } else if (ch == ',') {
      fields.push_back(field);
      field.clear();
    } else if (ch != '\r') {
      field += ch;
    }
  }
  if (quoted) return false;
  fields.push_back(field);
  return true;
}

bool integer(const std::string& text, int64_t& value) {
  if (text.empty()) return false;
  errno = 0;
  char* end = nullptr;
  const long long parsed = std::strtoll(text.c_str(), &end, 10);
  if (errno != 0 || end == text.c_str() || *end != '\0') return false;
  value = parsed;
  return true;
}

template <typename Callback>
LoadResult rows(const char* name, const std::string& csv, const char* header, Callback callback) {
  std::istringstream input(csv);
  std::string line;
  int number = 0;
  if (!std::getline(input, line) || (line != header && line != std::string(header) + "\r")) {
    return {false, 1, name, "missing or unsupported header"};
  }
  ++number;
  std::vector<std::string> fields;
  while (std::getline(input, line)) {
    ++number;
    if (line.empty() || line == "\r") continue;
    if (!split(line, fields)) return {false, number, name, "invalid CSV quoting"};
    const char* error = callback(fields);
    if (error) return {false, number, name, error};
  }
  return {true, 0, {}, {}};
}

}  // namespace

CsvFiles writeCsv(const Model& model) {
  CsvFiles out;
  out.habits = "id,name,created_day,order\n";
  for (const auto& habit : model.habits()) {
    out.habits += std::to_string(habit.id) + ',' + quote(habit.name) + ',' + std::to_string(habit.createdDay) + ',' +
                  std::to_string(habit.order) + '\n';
  }
  out.tasks = "id,habit_id,name,created_day,retired_day,order\n";
  for (const auto& task : model.tasks()) {
    out.tasks += std::to_string(task.id) + ',' + std::to_string(task.habitId) + ',' + quote(task.name) + ',' +
                 std::to_string(task.createdDay) + ',' + std::to_string(task.retiredDay) + ',' +
                 std::to_string(task.order) + '\n';
  }
  out.schedules = "kind,habit_id,effective_day,value\n";
  for (const auto& revision : model.schedules()) {
    out.schedules += "schedule," + std::to_string(revision.habitId) + ',' + std::to_string(revision.effectiveDay) + ',' +
                     std::to_string(revision.weekdayMask) + '\n';
  }
  for (const auto& revision : model.activeRevisions()) {
    out.schedules += "active," + std::to_string(revision.habitId) + ',' + std::to_string(revision.effectiveDay) + ',' +
                     (revision.active ? "1\n" : "0\n");
  }
  out.completions = "habit_id,task_id,day,complete\n";
  for (const auto& completion : model.completions()) {
    out.completions += std::to_string(completion.habitId) + ',' + std::to_string(completion.taskId) + ',' +
                       std::to_string(completion.day) + ',' + (completion.complete ? "1\n" : "0\n");
  }
  out.exceptions = "habit_id,day,skipped\n";
  for (const auto& exception : model.exceptions()) {
    out.exceptions += std::to_string(exception.habitId) + ',' + std::to_string(exception.day) + ',' +
                      (exception.skipped ? "1\n" : "0\n");
  }
  return out;
}

LoadResult readCsv(const CsvFiles& files, Model& model) {
  Model loaded;
  int64_t v[6] = {};
  auto result = rows("habits.csv", files.habits, "id,name,created_day,order", [&](const auto& f) -> const char* {
    if (f.size() != 4 || !integer(f[0], v[0]) || !integer(f[2], v[2]) || !integer(f[3], v[3])) return "invalid habit row";
    return loaded.restoreHabit({static_cast<Id>(v[0]), f[1], static_cast<int>(v[2]), static_cast<int>(v[3])}) ? nullptr
                                                                                                             : "invalid habit";
  });
  if (!result.ok) return result;
  result = rows("tasks.csv", files.tasks, "id,habit_id,name,created_day,retired_day,order", [&](const auto& f) -> const char* {
    if (f.size() != 6 || !integer(f[0], v[0]) || !integer(f[1], v[1]) || !integer(f[3], v[3]) ||
        !integer(f[4], v[4]) || !integer(f[5], v[5])) return "invalid task row";
    return loaded.restoreTask({static_cast<Id>(v[0]), static_cast<Id>(v[1]), f[2], static_cast<int>(v[3]),
                               static_cast<int>(v[4]), static_cast<int>(v[5])})
               ? nullptr : "invalid task";
  });
  if (!result.ok) return result;
  result = rows("schedules.csv", files.schedules, "kind,habit_id,effective_day,value", [&](const auto& f) -> const char* {
    if (f.size() != 4 || !integer(f[1], v[1]) || !integer(f[2], v[2]) || !integer(f[3], v[3])) return "invalid schedule row";
    if (f[0] == "schedule") return loaded.restoreSchedule({static_cast<Id>(v[1]), static_cast<int>(v[2]), static_cast<uint8_t>(v[3])}) ? nullptr : "invalid schedule";
    if (f[0] == "active") return loaded.restoreActive({static_cast<Id>(v[1]), static_cast<int>(v[2]), v[3] != 0}) ? nullptr : "invalid active revision";
    return "unknown schedule kind";
  });
  if (!result.ok) return result;
  result = rows("completions.csv", files.completions, "habit_id,task_id,day,complete", [&](const auto& f) -> const char* {
    if (f.size() != 4 || !integer(f[0], v[0]) || !integer(f[1], v[1]) || !integer(f[2], v[2]) || !integer(f[3], v[3])) return "invalid completion row";
    return loaded.restoreCompletion({static_cast<Id>(v[0]), static_cast<Id>(v[1]), static_cast<int>(v[2]), v[3] != 0}) ? nullptr : "invalid completion";
  });
  if (!result.ok) return result;
  result = rows("exceptions.csv", files.exceptions, "habit_id,day,skipped", [&](const auto& f) -> const char* {
    if (f.size() != 3 || !integer(f[0], v[0]) || !integer(f[1], v[1]) || !integer(f[2], v[2])) return "invalid exception row";
    return loaded.restoreException({static_cast<Id>(v[0]), static_cast<int>(v[1]), v[2] != 0}) ? nullptr : "invalid exception";
  });
  if (!result.ok) return result;
  model = std::move(loaded);
  return {true, 0, {}, {}};
}

}  // namespace habits
