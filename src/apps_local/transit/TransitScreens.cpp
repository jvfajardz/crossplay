#include "TransitScreens.h"

#include "../ui/ToyboxMetrics.h"

namespace transitui {

namespace {

constexpr int16_t kTabY = 72;
constexpr int16_t kTabH = 50;
constexpr int16_t kPanelTop = 142;
constexpr int16_t kPanelGap = 12;
constexpr int16_t kFooterH = 52;

void drawDirection(toybox::Screen& screen, const fui::Rect& panel, const DirectionModel& model) {
  fui::TextStyle heading = screen.theme().titleText;
  heading.font = toybox::kBodyFont;
  heading.color = fui::Color::Black;
  heading.align = fui::TextAlign::Left;
  screen.target().text(fui::makeRect(panel.x, panel.y, panel.width, 32), model.heading, heading);

  const int16_t rowTop = static_cast<int16_t>(panel.y + 38);
  const int16_t rowH = static_cast<int16_t>((panel.height - 38) / 4);
  fui::TextStyle time = screen.theme().bodyText;
  time.font = toybox::kTileFont;
  time.align = fui::TextAlign::Center;

  for (int i = 0; i < 4; ++i) {
    const fui::Rect row = fui::makeRect(panel.x, static_cast<int16_t>(rowTop + i * rowH), panel.width, rowH);
    if (i < model.count && model.times[i] != nullptr) {
      screen.target().text(row, model.times[i], time);
    } else {
      fui::TextStyle empty = screen.theme().smallText;
      empty.align = fui::TextAlign::Center;
      screen.target().text(row, "--", empty);
    }
  }
}

}  // namespace

void build(toybox::Screen& screen, const Model& model) {
  toybox::absoluteChrome(screen);

  fui::HeaderProps header;
  header.title = "TRANSIT";
  header.rightLabel = model.updated;
  header.subtitleText = screen.theme().smallText;
  header.subtitleText.color = fui::Color::White;
  header.subtitleText.align = fui::TextAlign::Right;
  header.borderEdges = fui::EdgesNone;
  toybox::headerBand(screen, header);

  const fui::DeviceContext& device = screen.frame().device();
  constexpr const char* labels[3] = {"306", "M52", "T26"};
  constexpr fui::ActionId actions[3] = {ActionBus306, ActionMetro52, ActionTram26};
  const int16_t usableW = static_cast<int16_t>(device.width - 2 * toybox::kMargin);
  const int16_t tabW = static_cast<int16_t>((usableW - 2 * toybox::kGutter) / 3);
  for (int i = 0; i < 3; ++i) {
    fui::ButtonProps tab;
    tab.label = labels[i];
    tab.action = i == model.selectedTab ? fui::NO_ACTION : actions[i];
    tab.styles = i == model.selectedTab ? toybox::invertedStyles() : toybox::rowStyles();
    tab.radius = static_cast<uint8_t>(toybox::kPillRadius);
    screen.button(
        tab, fui::makeRect(static_cast<int16_t>(toybox::kMargin + i * (tabW + toybox::kGutter)), kTabY, tabW, kTabH));
  }

  if (model.notice != nullptr) {
    fui::TextStyle notice = screen.theme().bodyText;
    notice.align = fui::TextAlign::Center;
    notice.maxLines = 4;
    screen.target().text(fui::makeRect(toybox::kMargin, 230, usableW, 250), model.notice, notice);
  } else {
    const int16_t panelW = static_cast<int16_t>((usableW - kPanelGap) / 2);
    const int16_t panelH = static_cast<int16_t>(device.height - kPanelTop - kFooterH - 2 * toybox::kMargin);
    drawDirection(screen, fui::makeRect(toybox::kMargin, kPanelTop, panelW, panelH), model.directions[0]);
    drawDirection(screen,
                  fui::makeRect(static_cast<int16_t>(toybox::kMargin + panelW + kPanelGap), kPanelTop, panelW, panelH),
                  model.directions[1]);
  }

  fui::ButtonProps refresh;
  refresh.label = "REFRESH";
  refresh.action = ActionRefresh;
  refresh.styles = toybox::rowStyles();
  refresh.radius = static_cast<uint8_t>(toybox::kPillRadius);
  screen.button(
      refresh, fui::makeRect(toybox::kMargin, static_cast<int16_t>(device.height - kFooterH - toybox::kMargin), usableW,
                             kFooterH));
}

}  // namespace transitui
