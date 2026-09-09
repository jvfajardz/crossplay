#pragma once

#include "../ui/ToyboxScreen.h"

namespace transitui {

namespace fui = freeink::ui;

enum : fui::ActionId {
  ActionBus306 = 700,
  ActionMetro52 = 701,
  ActionTram26 = 702,
  ActionRefresh = 703,
};

struct DirectionModel {
  const char* heading = "";
  const char* times[4] = {};
  int count = 0;
};

struct Model {
  int selectedTab = 0;
  const char* updated = "";
  const char* notice = nullptr;
  DirectionModel directions[2];
};

void build(toybox::Screen& screen, const Model& model);

}  // namespace transitui
