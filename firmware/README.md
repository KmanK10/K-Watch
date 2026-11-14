# K-Watch firmware

Build from this directory with PlatformIO and the ESP-IDF framework.

| Watch | PlatformIO environment | Power / vibration |
| --- | --- | --- |
| LilyGo T-Watch S3 | `twatch-s3` (default) | AXP2101 / DRV2605 |
| LilyGo T-Watch 2020 **V3** | `twatch-2020-v3` | AXP202 / GPIO4 motor |

The 2020 target is specifically for V3 (the microphone revision). V1 and V2
have different power wiring and sensor orientations and are not supported by
this target.

## Build and flash

```sh
pio run -e twatch-2020-v3
pio run -e twatch-2020-v3 -t upload
pio device monitor -e twatch-2020-v3
```

Select the `twatch-2020-v3` environment in the PlatformIO IDE for this watch.
Replace that environment with `twatch-s3` for the S3. Both targets use the
existing 16MB flash partition layout. The V3 console uses its USB-to-UART
bridge at 115200 baud; the S3 uses native USB.

Common ESP-IDF settings are in `sdkconfig.defaults`; settings for each board
are in `config/`. Generated `sdkconfig.<environment>` files retain local
menuconfig choices. To apply changed defaults to an existing build, back up
and remove its generated sdkconfig, then run `pio run -e <environment> -t clean`
and rebuild.

## 2020 V3 hardware

The target configures the ST7789 display, FT6336 touch/reset, AXP202 battery
and button events, PCF8563 RTC, BMA423 steps/wrist tilt, and timed motor
patterns. Speaker, microphone, and IR functionality are not implemented.
The AXP202 charger is configured for 4.2V and its minimum 300mA charge current.
The board has 8MB PSRAM, of which the original ESP32 directly maps 4MB for
normal heap allocation.

The original ESP32 cannot maintain Bluetooth using its main crystal in light
sleep. ESP-IDF therefore blocks automatic light sleep while the Bluetooth
controller is enabled; CPU frequency scaling and Bluetooth modem sleep still
work. The current Bluetooth toggle stops advertising/connections but leaves
the controller initialized, so it does not remove this restriction. This
differs from the S3 and affects battery life.

Pin assignments, power domains, and sensor orientation follow LilyGo's
[V3 hardware documentation](https://github.com/Xinyuan-LilyGO/TTGO_TWatch_Library/blob/master/docs/watch_2020_v3.md)
and [reference initialization](https://github.com/Xinyuan-LilyGO/TTGO_TWatch_Library/blob/master/src/TTGO.h).
The AXP202 register definitions follow the
[LilyGo AXP20X driver](https://github.com/Xinyuan-LilyGO/TTGO_TWatch_Library/blob/master/src/drive/axp/axp20x.cpp).

## Verification

```sh
python3 -m unittest discover -s tests
pio run -e twatch-2020-v3
pio run -e twatch-s3
```

The host test compiles the AXP202 driver against simulated I2C registers and
checks power/charging setup, interrupt decoding, battery status, and failures.
It requires Python 3 and a C compiler (`cc`, or set `CC`).

Hardware validation is still required: check display/touch alignment, button
wake, charging/USB events, vibration, step counting, wrist wake, Bluetooth
pairing/reconnection, and sleep/wake with Bluetooth both on and off.
