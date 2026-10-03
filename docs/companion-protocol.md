# K-Watch companion protocol

How the iPhone companion app (`ios/`) talks to the watch (`firmware/`). Notifications, music
and the time don't use this: they come from iOS's built-in ANCS, AMS and CTS services and work
without the app. This protocol is for everything else: weather, settings, health data,
diagnostics, and whatever is added later.

Protocol version: **1**

## Bluetooth

The watch is a BLE peripheral with one custom GATT service:

| What | UUID | Properties |
|---|---|---|
| Service | `58250B93-E11B-42C4-8104-7DDC9BD61EB2` | |
| RX (app to watch) | `58250B94-E11B-42C4-8104-7DDC9BD61EB2` | Write, Write Without Response |
| TX (watch to app) | `58250B95-E11B-42C4-8104-7DDC9BD61EB2` | Notify |

Both characteristics need an encrypted link. The watch is already paired and bonded with the
iPhone for notifications, so iOS encrypts automatically; the app never sees pairing.

### Connecting from iOS

The watch is normally already connected to the iPhone at the system level (for notifications),
so it won't show up in a scan. Instead:

1. `centralManager.retrieveConnectedPeripherals(withServices: [service])` returns the watch.
2. `connect(_:)` it. This shares the existing link; nothing visible happens on the watch.
3. Discover the service and both characteristics, then `setNotifyValue(true, for: tx)`.
4. Send `hello` (below).

If the watch isn't connected (out of range, Bluetooth off), fall back to scanning for the
service UUID: the watch puts it in its scan response. A pending `connect(_:)` on a known
peripheral never times out on iOS, so the app can keep one open and get called back whenever
the watch comes into range, even in the background.

The app should use the `bluetooth-central` background mode and Core Bluetooth state
restoration. While the app is subscribed to TX, a notification from the watch wakes the app in
the background; the watch uses this to ask for fresh weather (`weather.request`).

### Framing

Messages are UTF-8 JSON objects, usually longer than one BLE packet, so each one is split into
chunks. Every write to RX and every notification on TX is one chunk:

```
byte 0     flags: bit 0 = START (first chunk of a message), bit 1 = END (last chunk)
byte 1...  the next piece of the message
```

A message that fits in one chunk has flags `0x03`. The receiver appends chunk data from a START
to an END; a new START throws away an unfinished message. Messages are at most 8 KB.

Chunk size: at most the ATT MTU minus 3, including the flags byte. On iOS,
`peripheral.maximumWriteValueLength(for: .withResponse)` gives the room for one write; use that
minus 1 for the data. Prefer `.withResponse` writes: messages are small, and iOS then handles
flow control.

## Messages

Every message has a type in `"t"`. Requests from the app carry an `"id"` (any integer the app
chooses); the watch's reply has the same `"t"`, `"re"` set to that id, and `"ok"`:

```json
{"t": "settings.get", "id": 7}
{"t": "settings.get", "re": 7, "ok": true, "settings": {...}}
{"t": "nonsense", "re": 8, "ok": false, "error": "unknown type"}
```

Messages the watch sends on its own (events) have no `"re"`. Both sides ignore fields they don't
know, so new fields can be added without breaking older versions. `protocol` only goes up for
changes that would break an older app or watch.

Times are Unix timestamps in seconds. Temperatures and other values are already in the units
the user wants; the watch displays them as given.

**Time zone.** The watch gets the local time from the iPhone (Current Time Service), but not
its time zone. Put `"utc_offset"` (seconds east of UTC, i.e.
`TimeZone.current.secondsFromGMT()`) at the top level of `hello` and `weather.set`. Until the
watch has it, it can't line Unix times up with its clock, so weather ages and sun/moon times
are off by the offset. The watch remembers it across restarts.

### hello

The app sends this first, after subscribing to TX.

```json
{"t": "hello", "id": 1, "app": "1.0", "protocol": 1, "utc_offset": -25200}
```

Reply:

```json
{"t": "hello", "re": 1, "ok": true, "protocol": 1, "name": "K-Watch",
 "device": "LilyGo T-Watch S3", "firmware": "v0.4-12-gabcdef",
 "features": ["weather", "settings", "health", "diagnostics", "find"]}
```

If the watch's weather is older than 30 minutes, it follows up with a `weather.request`.

### weather.set

App to watch. Replaces the watch's weather; it's kept across restarts.

```json
{"t": "weather.set", "id": 2, "utc_offset": -25200, "weather": {
  "updated": 1790000000,
  "location": "Seattle",
  "lat": 47.61, "lon": -122.33,
  "unit": "F",
  "now":   {"temp": 57, "feels": 55, "code": 3, "day": true, "humidity": 80, "wind": 5,
            "uv": 2.4, "aqi": 38},
  "today": {"high": 61, "low": 49, "precip": 40, "sunrise": 1789990000, "sunset": 1790030000},
  "hours": [{"time": 1790002800, "temp": 57, "code": 3, "precip": 10}],
  "days":  [{"time": 1789974000, "high": 61, "low": 49, "code": 61, "precip": 80}]
}}
```

- `updated`: when the forecast was fetched.
- `lat`, `lon`: where the forecast is for (two decimals is plenty). The watch computes dawn,
  sunrise, sunset, dusk and the moon's rise, set and phase itself from these, so the app
  doesn't need to send them, and the Moon app keeps working while the phone is away.
- `unit`: `"F"` or `"C"`, for display. Temperatures are whole numbers in that unit. `wind` is
  mph with `"F"` and km/h with `"C"`.
- `code`: a WMO weather interpretation code, as returned by Open-Meteo's `weather_code`
  (0 clear, 1-3 mainly clear to overcast, 45/48 fog, 51-57 drizzle, 61-67 rain, 71-77 snow,
  80-82 showers, 85/86 snow showers, 95-99 thunderstorm).
- `day`: false at night, so the watch can show a moon instead of a sun.
- `precip`: chance of precipitation in percent.
- `uv`: the current UV index (Open-Meteo forecast API, `current=uv_index`); the watch rounds it.
- `aqi`: the current US AQI (0-500) from Open-Meteo's separate Air Quality API
  (`https://air-quality-api.open-meteo.com/v1/air-quality?current=us_aqi`).
- `hours`: up to 12, starting with the next hour. `days`: up to 7, starting today; `time` is the
  start of that day.
- Any field can be left out if unknown.

Reply: `{"t": "weather.set", "re": 2, "ok": true}`

### weather.request

Watch to app (event). Asks the app to send `weather.set`.

```json
{"t": "weather.request", "reason": "stale"}
```

`reason` is `"stale"` (weather older than 30 minutes; repeated every 30 minutes while it stays
stale and the app is connected) or `"user"` (refresh tapped in the Weather app).

### settings.get / settings.set

```json
{"t": "settings.get", "id": 3}
{"t": "settings.set", "id": 4, "settings": {"brightness": 80, "dnd": true}}
```

Both reply with every setting:

```json
{"t": "settings.get", "re": 3, "ok": true, "settings": {
  "brightness": 60, "screen_timeout": 5, "raise_to_wake": true, "tap_to_wake": true,
  "notify_vibrate": true, "touch_feedback": true, "clock_24h": false, "dnd": false,
  "bluetooth": true, "step_goal": 8000
}}
```

| Setting | Values |
|---|---|
| `brightness` | 10-100 (percent) |
| `screen_timeout` | seconds: 5, 10, 15 or 30 |
| `raise_to_wake`, `tap_to_wake` | wake the screen by raising the wrist / tapping it |
| `notify_vibrate` | buzz for notifications |
| `touch_feedback` | clicks for taps, scroll wheels and dragging |
| `clock_24h` | 24-hour clock |
| `dnd` | Do not disturb (always off after a restart) |
| `bluetooth` | read only; turning it off would cut off the app |
| `step_goal` | daily step goal, 1000-50000 |

`settings.set` only changes the settings it includes. Out-of-range values are clamped. When a
setting is changed on the watch itself, it sends:

```json
{"t": "settings.changed", "settings": {...every setting...}}
```

### health.get

```json
{"t": "health.get", "id": 5}
```

Reply:

```json
{"t": "health.get", "re": 5, "ok": true, "date": "2026-10-03", "today": 4321, "goal": 8000,
 "days": [9120, 6480, null, 7350]}
```

`date` is the watch's current day. `days[0]` is yesterday, `days[1]` the day before, up to 30
days back; `null` means the watch has no count for that day (it was off, or before the watch
started keeping history).

### diag.get

```json
{"t": "diag.get", "id": 6}
```

Reply:

```json
{"t": "diag.get", "re": 6, "ok": true,
 "firmware": "v0.4-12-gabcdef", "uptime": 86400, "time": 1790000000,
 "restart_reason": "power-on",
 "heap_free": 180000, "heap_min": 150000, "psram_free": 8000000,
 "battery": {"percent": 85, "voltage": 4010, "charging": false, "usb": false},
 "notifications": 4}
```

- `restart_reason`: `"power-on"`, `"software"` (flashed, or factory reset), `"crash"`,
  `"froze"` (watchdog), `"brownout"` (battery voltage dipped), `"deep-sleep"` or `"other"`.
- `heap_min`: the lowest free memory since the restart.
- `notifications`: how many are in the watch's list.
- `battery.percent` is -1 when the watch can't tell (no battery connected).

### find

Makes the watch buzz and light up, for finding it.

```json
{"t": "find", "id": 7}
```

Reply: `{"t": "find", "re": 7, "ok": true}`

### Planned

Not implemented yet; listed so names don't clash.

- `battery.history`: battery level and screen-on time over the last days, for battery usage
  reports (once the watch has a real battery).
- `alarms.get` / `alarms.set`: edit alarms from the phone.
- `log.get`: recent firmware log lines, for bug reports.
