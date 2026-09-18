# Xteink X4 Pro app feasibility guide

Use this guide as the feasibility gate and starting point for every custom app.
These constraints come from the hardware-tested Transit project. Requirements
may depart from a default only deliberately; hard constraints require an
explicit technical reason and user-visible tradeoff.

## Hard device constraints

| Area | Constraint |
| --- | --- |
| Target | Xteink X4 Pro, ESP32-S3, 16 MB flash, 8 MB PSRAM |
| Logical portrait canvas | 480 x 800 pixels |
| Measured active display | Approximately 53.5 x 89 mm |
| Approximate density | 228 PPI |
| Rendering | E-ink; favor stable pages and deliberate refreshes |
| Device fonts | Use bundled font IDs only; desktop/web fonts are not available |
| Icons | Use verified built-in icons or draw geometry; do not assume Unicode glyph coverage |
| Internal time | UTC from NTP/hardware RTC |
| Local display time | Convert explicitly to `Europe/Amsterdam` where required |
| Build target | PlatformIO environment `x4pro`; never substitute an ESP32-C3 build |
| Generated artifacts | Keep in `dist/`; never commit them |

## Bundled font baseline

Known font IDs are declared in `src/fontIds.h`:

- `UI_10_FONT_ID` and `UI_12_FONT_ID` for compact interface text;
- `NOTOSANS_12_FONT_ID`, `NOTOSANS_14_FONT_ID`,
  `NOTOSANS_16_FONT_ID`, and `NOTOSANS_18_FONT_ID`;
- `NOTOSERIF_12_FONT_ID`, `NOTOSERIF_14_FONT_ID`,
  `NOTOSERIF_16_FONT_ID`, and `NOTOSERIF_18_FONT_ID`.

Measure text and line height through the firmware renderer. Do not infer device
metrics from CSS. Use bold to direct attention, and verify every non-ASCII glyph
on the actual font. The Transit refresh glyph rendered incorrectly until it was
replaced with a vector-drawn icon.

## Layout defaults

These are tested defaults, not immutable requirements:

- portrait orientation;
- renderer `safeRect()` as the layout boundary;
- approximately 14 px page margins;
- approximately 68 px header when a title and metadata share the header;
- controls around 44 px high or larger;
- one-screen information architecture when practical;
- no animation or interaction that depends on rapid repaints.

The physical preview calibration established during Transit testing is:

- Dell U2414H at 1920 x 1080;
- Windows display scaling at 100%;
- Chrome page zoom at 75%;
- preview display area measuring 5.35 x 8.9 cm.

A different monitor requires a new physical calibration. The preview must still
use a 480 x 800 logical canvas even when its CSS size changes.

## Memory and network rules

The ESP32-S3 has limited internal RAM even when PSRAM is present. A station-wide
Transitous response of about 172 KB caused an abort while fetching/parsing live
departures. Therefore:

- request the smallest useful response;
- prefer route-, stop-, and time-targeted queries;
- apply conservative response byte limits;
- use filtered or streaming JSON parsing;
- avoid retaining a large raw payload and a full parsed tree simultaneously;
- handle download, allocation, and parsing failures without rebooting;
- test against current production payload sizes, not only sample fixtures;
- show a clear stale/error state when live data cannot be trusted.

## Time and timezone rules

Keep these concepts separate:

```text
hardware RTC / system epoch     UTC
timestamp parsing               source-declared timezone
timezone-less Dutch transit API Europe/Amsterdam
user-facing local time          Europe/Amsterdam with DST
status-bar UTC offset           display preference only
```

Never use the status-bar UTC offset as application time state. Validate that the
clock is plausible before time-dependent filtering. APIs that omit an offset
must have their documented/source timezone applied explicitly.

## E-ink interaction rules

- Avoid continuous clocks, animation, spinners, and unnecessary polling.
- Keep each refresh intentional and tolerate visible update latency.
- Prefer a static loading message followed by one completed repaint.
- Use physical buttons and touch targets consistently with existing CrossPlay
  navigation.
- Render cancellation, delay, selection, and error states without relying on
  color alone.
- Verify text baselines, bold weight, rules, and strikethroughs on device.

## Feasibility review for every new requirement

Before implementation, answer:

1. Does it fit a 480 x 800 static layout at readable physical size?
2. Are every requested font and glyph available on device?
3. Can its peak response and parsed representation fit safely in memory?
4. Does it require reliable time, timezone, Wi-Fi, credentials, or an API whose
   assumptions need to be made explicit?
5. Can failure be shown without a reboot, blank page, or misleading stale data?
6. Can the feature be previewed realistically before flashing?
7. Does it remain isolated enough to live on its own `app-<name>` branch?

If any answer is no or unknown, pause that part of the design, explain the
constraint, and propose a smaller or safer version.

## Standard project sequence

1. Check upstream status without automatically adopting it.
2. Create `app-<name>` from `xteink` and merge `project-rules`.
3. Clarify the app's unique data, screens, and interactions.
4. Build a calibrated preview for visual review.
5. Implement the smallest complete version under `src/apps_local/<name>/`.
6. Run targeted checks and build `x4pro`.
7. Merge and validate the full ESP32-S3 image.
8. Publish a test installer only when requested.
9. Obtain hardware confirmation before committing final app changes.
10. Push the standalone branch and integrate it into `custom-apps`.

