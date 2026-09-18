#pragma once

#include <string>

#include "HabitModel.h"

namespace habits {

struct CsvFiles {
  std::string habits;
  std::string tasks;
  std::string schedules;
  std::string completions;
  std::string exceptions;
};

struct LoadResult {
  bool ok = false;
  int line = 0;
  std::string file;
  std::string message;
};

CsvFiles writeCsv(const Model& model);
LoadResult readCsv(const CsvFiles& files, Model& model);

}  // namespace habits
