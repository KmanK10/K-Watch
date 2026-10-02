# K-Watch plan

Goal: a smartwatch that shows iPhone notifications and lasts 24+ hours (ideally 2-4 days),
first on the LilyGo T-Watch S3, later on a custom STM32 board.

## Repo layout

- `firmware/` - watch firmware (PlatformIO + ESP-IDF, C, LVGL, NimBLE)
- `ios/` - companion iOS app (SwiftUI, needs a Mac with Xcode)
- `docs/` - plans, Bluetooth protocol notes, power measurements

The original Arduino prototype is preserved at git tag `prototype-v0`.

## Rules

1. **Power first.** Every feature gets measured before it's considered done.
2. **Event-driven, never polling.** The CPU sleeps until an interrupt (button, touch,
   wrist tilt, RTC alarm, Bluetooth) wakes it.
3. **Hardware stays in `firmware/src/hal/`.** UI and app logic never touch ESP-IDF
   drivers directly, so the STM32 port only rewrites `hal/` and the BLE glue.
4. **Compiler warnings on** (`-Wall -Wextra`) and treated seriously.

## Lessons from the prototype

- "Sleep" only turned the backlight off. The loop kept spinning at 240 MHz with no delay,
  reading the PMU and touch controller over I2C every iteration.
- Display controller, touch controller, radio and six BMA423 features stayed powered.
- `info.flag.minuteChanged;` (missing `= 1`) meant the clock only redrew on wake.
  `-Wall` would have flagged it.

## Power budget

Battery is a 502530 LiPo (5.0 x 25 x 30 mm), realistically ~300-350 mAh.
300 mAh / 24 h = ~12.5 mA average. Targets:

| State | Target |
|---|---|
| Idle, screen off, BLE connected | < 3 mA |
| Screen on | ~30-50 mA, a few minutes per day |

Measuring: the AXP2101 has a fuel gauge but no battery current reading, so the built-in
option is logging battery voltage/percent over several hours and estimating the average.
A cheap inline USB meter is better for quick before/after checks.

## Roadmap

0. Repo setup, toolchain, skeleton firmware - **done**
1. Low-power base: watch face, button / wrist-tilt wake, display + touch real power-down,
   automatic light sleep, current debug screen. Target < 3 mA idle.
2. BLE: pair with iPhone, ANCS notifications, CTS time sync, haptics.
3. AMS music control, notification list with dismiss/actions.
4. Alarms, timers, stopwatch, flashlight (RTC alarm wakes the watch).
5. Step counting on the BMA423 itself.
6. iOS companion app: weather, calendar, settings.
7. Port to the custom STM32 board (rewrite `hal/` + BLE glue).

## iPhone notes

- **ANCS** (Apple Notification Center Service) delivers all iPhone notifications to a bonded
  BLE device with no app installed. iOS apps cannot read other apps' notifications at all.
- **AMS** (Apple Media Service) for music control, **CTS** (Current Time Service) for time.
- The companion app is only needed for extras (weather, calendar, settings, firmware updates).
