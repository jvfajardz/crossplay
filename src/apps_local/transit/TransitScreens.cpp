#include "TransitScreens.h"

#include <cstdio>

#include "../ui/ToyboxMetrics.h"

namespace transitui {
namespace {

constexpr int16_t kHeaderH = 68;
constexpr int16_t kRefreshW = 62;
constexpr int16_t kPageMargin = 14;

void drawRefreshIcon(toybox::Screen& screen, const fui::Rect& box) {
  const int16_t cx = static_cast<int16_t>(box.x + box.width / 2);
  const int16_t cy = static_cast<int16_t>(box.y + box.height / 2);
  const fui::Paint white = fui::Paint::solid(fui::Color::White);
  const fui::Point points[] = {{static_cast<int16_t>(cx - 13), static_cast<int16_t>(cy - 2)},
                               {static_cast<int16_t>(cx - 9), static_cast<int16_t>(cy - 10)},
                               {cx, static_cast<int16_t>(cy - 14)},
                               {static_cast<int16_t>(cx + 10), static_cast<int16_t>(cy - 10)},
                               {static_cast<int16_t>(cx + 14), static_cast<int16_t>(cy - 2)},
                               {static_cast<int16_t>(cx + 10), static_cast<int16_t>(cy + 8)},
                               {static_cast<int16_t>(cx + 2), static_cast<int16_t>(cy + 13)}};
  for (size_t i = 1; i < sizeof(points) / sizeof(points[0]); ++i)
    screen.target().line(points[i - 1], points[i], 2, white);
  screen.target().line(points[0], {static_cast<int16_t>(cx - 14), static_cast<int16_t>(cy - 11)}, 2, white);
  screen.target().line(points[0], {static_cast<int16_t>(cx - 5), static_cast<int16_t>(cy - 4)}, 2, white);
}

void drawDirection(toybox::Screen& screen, const fui::Rect& row, const DirectionModel& model) {
  fui::TextStyle heading;
  heading.font = fui::FONT_SLOT_SMALL;
  heading.color = fui::Color::Black;
  screen.target().text(fui::makeRect(row.x, row.y + 3, row.width, 25), model.heading, heading);

  int16_t x = row.x;
  const int16_t y = static_cast<int16_t>(row.y + 29);
  fui::TextStyle dash = heading;
  dash.font = fui::FONT_SLOT_BODY;
  screen.target().text(fui::makeRect(x, y, 14, 31), "-", dash);
  x = static_cast<int16_t>(x + 15);

  for (int i = 0; i < model.count; ++i) {
    char item[32];
    std::snprintf(item, sizeof(item), "%s%s", model.times[i].label, i + 1 < model.count ? "," : "");
    fui::TextStyle time;
    time.font = fui::FONT_SLOT_BODY;
    time.color = fui::Color::Black;
    time.bold = model.times[i].highlighted;
    const fui::Size measured = screen.target().measureText(time.font, item, time);
    screen.target().text(fui::makeRect(x, y, measured.width + 7, 31), item, time);
    if (model.times[i].cancelled) {
      const int16_t strikeY = static_cast<int16_t>(y + screen.target().lineHeight(time.font) / 2);
      screen.target().line({x, strikeY}, {static_cast<int16_t>(x + measured.width - 4), strikeY}, 1,
                           fui::Paint::solid(fui::Color::Black));
    }
    x = static_cast<int16_t>(x + measured.width + 8);
    if (x >= row.right()) break;
  }

  screen.target().line({row.x, static_cast<int16_t>(row.bottom() - 1)},
                       {row.right(), static_cast<int16_t>(row.bottom() - 1)}, 1,
                       fui::Paint::solid(fui::Color::DarkGray));
}

}  // namespace

void build(toybox::Screen& screen, const Model& model) {
  toybox::absoluteChrome(screen);
  const fui::DeviceContext& device = screen.frame().device();
  const fui::Rect safe = device.safeRect();

  const fui::Rect header = fui::makeRect(safe.x, safe.y, safe.width, kHeaderH);
  screen.target().fill(header, fui::Paint::solid(fui::Color::Black));

  fui::TextStyle title;
  title.font = fui::FONT_SLOT_TITLE;
  title.color = fui::Color::White;
  title.bold = true;
  screen.target().text(fui::makeRect(header.x + kPageMargin, header.y + 4, header.width - kRefreshW - 24, 34),
                       "Transit Timetable", title);

  char clock[48];
  std::snprintf(clock, sizeof(clock), "updated %s, current time %s", model.updated[0] ? model.updated : "--:--",
                model.current[0] ? model.current : "--:--");
  fui::TextStyle meta;
  meta.font = fui::FONT_SLOT_SMALL;
  meta.color = fui::Color::White;
  screen.target().text(fui::makeRect(header.x + kPageMargin, header.y + 38, header.width - kRefreshW - 24, 25), clock,
                       meta);

  const fui::Rect refreshRect =
      fui::makeRect(static_cast<int16_t>(header.right() - kRefreshW - 8), header.y + 12, kRefreshW, 44);
  fui::ButtonProps refresh;
  refresh.label = "";
  refresh.action = ActionRefresh;
  refresh.styles = toybox::invertedStyles();
  refresh.radius = 0;
  screen.button(refresh, refreshRect);
  drawRefreshIcon(screen, refreshRect);

  const int16_t bodyTop = static_cast<int16_t>(header.bottom() + 2);
  if (model.notice != nullptr) {
    fui::TextStyle notice;
    notice.font = fui::FONT_SLOT_BODY;
    notice.align = fui::TextAlign::Center;
    notice.maxLines = 4;
    screen.target().text(fui::makeRect(safe.x + kPageMargin, bodyTop + 80, safe.width - 2 * kPageMargin, 220),
                         model.notice, notice);
    return;
  }

  if (model.directionCount <= 0) return;
  const int16_t rowH = static_cast<int16_t>((safe.bottom() - bodyTop) / model.directionCount);
  for (int i = 0; i < model.directionCount; ++i) {
    drawDirection(screen,
                  fui::makeRect(safe.x + kPageMargin, static_cast<int16_t>(bodyTop + i * rowH),
                                safe.width - 2 * kPageMargin, rowH),
                  model.directions[i]);
  }
}

}  // namespace transitui
