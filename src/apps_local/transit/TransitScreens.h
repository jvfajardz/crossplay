#pragma once

#include "../ui/ToyboxScreen.h"

namespace transitui {

namespace fui = freeink::ui;

enum : fui::ActionId { ActionRefresh = 703 };

struct DepartureModel {
  const char* label = "";
  bool highlighted = false;
  bool cancelled = false;
};

struct DirectionModel {
  const char* heading = "";
  DepartureModel times[5] = {};
  int count = 0;
};

struct Model {
  const char* updated = "";
  const char* current = "";
  const char* notice = nullptr;
  DirectionModel directions[8];
  int directionCount = 0;
};

void build(toybox::Screen& screen, const Model& model);

}  // namespace transitui
