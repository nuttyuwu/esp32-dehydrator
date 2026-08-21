# Dehydrator firmware — ESP32

Board on the bench: **ESP32-D0WD-V3 rev 3.1, 4 MB flash, 40 MHz crystal**, MAC
`a0:b7:65:fe:72:70`, CH340 USB-serial — `board = esp32dev`, no PSRAM. Not a WROVER, and it
does not need PSRAM: the firmware peaks at 14 % of internal RAM. Moving to a real WROVER is
a two-line change in `platformio.ini`, documented there.

Firmware for the hybrid solar / electric food dryer. Solar collector preheats intake air, an
electric heater covers the shortfall around a 44 °C setpoint, fans force circulation, two
SHT31s measure T/RH at the cabinet bottom and top, and the board hosts its own Wi-Fi AP with
a live-numbers page and CSV export.

Spec and scope: [ARCHITECTURE.md](ARCHITECTURE.md). Read section 0 before anything else — it
carries the deliverable boundary, the thermal-fuse responsibility, the support window, and
the code rights.

> **Build status: flashed and running.** Both firmware builds compile clean, 46/46 host
> tests pass, and 10 of the 11 acceptance criteria are verified — the host suite for the
> control logic, the board itself for NVS persistence and session recovery. The one
> remaining gap is the web page, which needs a phone or laptop on the AP to confirm.
> See [Verification status](#verification-status).

---

## Two builds

| | `env:sim` | `env:hw` |
|---|---|---|
| Sensors | `SimSensors` reading the plant model | two real SHT31s |
| Actuators | `SimActuators` feeding the plant model | GPIO relays / SSR |
| Serial TUI | yes | compiled out |
| Wi-Fi, web page, LittleFS, CSV | real | real |

`src/core/` and `src/net/` are byte-identical between them. The sim build runs to completion
on a bare ESP32 dev board with nothing attached — that is where the control law, the fault
trips, the logging, and the whole web UI get validated before a wire is crimped.

`env:native` compiles `core/` and `sim/` on the host for the unit tests. No Arduino.

---

## Working in VS Code

Install the **PlatformIO IDE** extension (already done on this machine; `.vscode/` is
committed so it configures itself). Everything below is on the blue status bar at the
bottom of the window — no terminal needed:

| Status bar | What it does |
|---|---|
| ✓ | Build the default env (`sim`) |
| → | Upload firmware to the board |
| 🔌 | Open the serial monitor — this is the TUI |
| 🗑 | Clean |
| PlatformIO icon (left sidebar) → **Devices** | Lists COM ports so you can see the board is detected |

The env switcher is on the same status bar; `sim` is the default, switch to `hw` when the
sensors are wired. Uploading the web page is the one thing without a button — sidebar →
**Project Tasks → sim → Platform → Upload Filesystem Image**.

**Serial monitor as a console**: `monitor_echo` and `monitor_filters = send_on_enter` are
set, so you can type a TUI command into the monitor pane, see what you typed, and it is
sent when you press Enter.

`monitor_dtr = 0` / `monitor_rts = 0` are also set and must stay that way. RTS drives EN on
this board, and pyserial asserts RTS on open — with the defaults the monitor holds the chip
in reset and shows a blank console that looks exactly like dead firmware.

### If an upload will not start

This board auto-resets and needs no button. An earlier ESP-AT board did, and the diagnosis
is worth keeping because the symptom is identical to a dozen unrelated problems.

`Wrong boot mode detected (0x13)` means **IO0 is not reaching the chip**. The give-away is
that esptool reports a boot *mode* at all — to do that it has to read the ROM log, so RX
and EN are both fine and only the boot-select strap is missing. On that board, driving
both modem lines through every combination with 500 ms of slack reset the chip every time
and never once produced `boot:0x03`: EN was wired to the adapter, IO0 only to the button.

- Board with a BOOT button → hold it down *through the whole connect phase*, release when
  `Writing at 0x…` appears. Tapping between attempts does nothing; esptool resets at the
  start of each attempt and IO0 has to already be low.
- No BOOT button → jumper **IO0 to GND**, flash, remove it afterwards.
- "Enter download mode first, flash later" does **not** work: pyserial asserts RTS when it
  opens the port, which pulses EN and knocks the chip straight back out.

`The chip stopped responding` partway through `flash_defl_block` is a different fault —
that is the USB-serial adapter losing sync at speed. Drop `upload_speed` to 115200.

### Driver note

This board uses a **CH340** USB-serial chip (`VID_1A86 PID_7523`). Windows has no inbox
driver for it — without the WCH driver installed, no COM port appears at all. Installed
here from WCH's own package (CH341SER 4.0, WHQL-signed by Microsoft), published as
`oem10.inf`. On a fresh machine, grab it from <https://www.wch-ic.com/downloads/CH341SER_ZIP.html>.

## Getting it running (sim, no hardware)

```bash
pip install platformio          # once

pio run  -e sim -t upload       # firmware
pio run  -e sim -t uploadfs     # web page into LittleFS  (required — the page lives there)
pio device monitor -b 115200    # the TUI
```

Then join Wi-Fi `DEHYDRATOR-XXXX` (password `[REDACTED]`) and open
<http://192.168.4.1>.

`uploadfs` is not optional. Without it LittleFS has no `/web/index.html` and every page
request redirects to a 404.

Host tests, no board needed:

```bash
pio test -e native
```

---

## Wiring summary

Full pin map and rationale: [ARCHITECTURE.md](ARCHITECTURE.md) §2. Pin constants live in
[src/hal/Pins.h](src/hal/Pins.h) — that file is the single source of truth for the diagram.

| Function | GPIO |
|---|---|
| I²C SDA / SCL | 21 / 22 |
| SHT31 bottom (inlet) | addr **0x44**, ADDR pin to GND |
| SHT31 top (exhaust) | addr **0x45**, ADDR pin to VCC |
| Heater SSR | 27 |
| Intake fan A / B | 32 / 33 |
| Stack fan (up-flow) | 25 |
| Klixon / limit switch | 34 (input-only, **needs an external pull-up**) |
| Status LED | 2 |
| Buzzer | 4 |

Three things to get right on the drawing:

1. **GPIO 16 and 17.** Free on this board, but reserved for PSRAM on a WROVER. The pin
   map avoids them either way, so the same wiring works on both — keep it that way.
2. **Relay polarity.** `kRelayActiveLow` in `Pins.h` defaults to `true` because most cheap
   relay boards energise on a LOW input. Check yours with a multimeter before the element is
   on mains — get this backwards and the heater runs whenever the ESP32 is in reset.
3. **The thermal fuse** (~70–80 °C) sits in series with the heater element, independent of
   the ESP32. The firmware's 65 °C trip is the second line of defence. It is not the first.

---

## TUI cheat sheet

115200 baud, one command per line. `help` prints this list on the device.

```
show                       one-shot state dump
mon on|off                 stream one status line per second

set setpoint 44.0          live config (also hyst, rh_target, loginterval, minon, minoff)
save                       persist config to NVS

mode off|auto|manual
start лууван               begin a session with a product label
stop                       stop -> cooldown
reset                      clear a latched fault
manual heater|fan_in|fan_up on|off

sim amb 22                 ambient temperature
sim solar 0.6              collector irradiance, 0..1
sim moisture 1.0           reset product moisture
sim speed 120              time acceleration for the plant model
sim t_bot 70               override a sensor reading (also rh_bot, t_top, rh_top)
sim clear                  drop all overrides
fault sensor bot|top|both|none
fault klixon on|off

log now | list | dump <file> [n] | seed <hours> | rm <file>
wifi                       ssid, ip, connected clients
```

`sim speed 120` is the one that matters: a six-hour drying run and its finish detection
verify in about three minutes.

`log seed <hours>` needs the clock set — open the web page once (it POSTs the browser's
epoch) or `POST /api/time`.

---

## Verification status

`pio test -e native` → **46 test cases, 46 passed**.
`pio run -e sim` / `-e hw` → **SUCCESS**, RAM 14.1 %, Flash 69.4 %, no warnings with `-Wall`.
Flashed to the board and driven through the TUI over serial.

| # | Acceptance criterion | Status | Evidence |
|---|---|---|---|
| 1 | Cold boot IDLE, outputs off, config from NVS | **verified on board** | `show` after reset: `state IDLE`, all outputs off, config loaded |
| 2 | Heater cycles ON ≤43.5 / OFF ≥44.5, dwell respected | **verified** | `test_controller` (6 cases) + `test_preheat_to_drying_then_cycles_in_band` |
| 3 | Manual heater with stack fan off stays off | **verified** | `test_actuator_interlock_…`, `test_manual_heater_without_stack_fan_stays_off` |
| 4 | `sim t_bot 70` latches over-temp | **verified on board** | tripped within one tick, `fault overtemp [LATCHED]`, heater off / stack fan on |
| 5 | Sensor fallback, then timeout fault | **verified** | `test_sensor_fallback_then_timeout`, `test_both_sensors_dead_trips_after_10s` |
| 6 | Heater-ineffective fault after 10 min | **verified** | `test_heater_that_does_not_heat_trips_after_10min` (+ two false-positive guards) |
| 7 | Full run finishes → COOLDOWN → DONE | **verified** | `test_run_finishes_by_itself` |
| 8 | CSV schema, rotation, download | **partial** | schema and rotation in `test_logger`; `log dump` on the board shows correct rows. HTTP download still unconfirmed |
| 9 | Setpoint survives power-cycle | **verified on board** | set 46.5 / band 2.0 → `save` → reset → `show` reported 46.5, band 45.5–47.5 |
| 10 | Reboot mid-run recovers the session | **verified on board** | `[boot] resuming session #1 (luuvan)`, back in PREHEAT with elapsed time carried |
| 11 | `pio test -e native` passes | **verified** | 46/46 |

Only the web UI is unconfirmed: join `DEHYDRATOR-7271`, open <http://192.168.4.1>, check the
page renders and a CSV downloads.

### Known wrinkle: timestamps before the clock is set

Until something sets the clock, rows are stamped `-uptime_s`. Uptime restarts at every
reboot, so a `nodate.csv` spanning reboots is **not monotonically ordered** and cannot be
sorted by time. Open the web page once at the start of a run — it POSTs the browser's epoch
— and every row after that carries a real timestamp in a correctly named daily file.

### Bugs the tests caught

Both were real, both are fixed, and neither would have been obvious by reading the code:

1. **Over-temp was being smoothed.** `Safety` read the median+EMA filtered temperature, so a
   jump to 70 °C took six ticks to cross the 65 °C trip line. Protective limits now read the
   raw value; the control loop still uses the filtered one.
2. **Outputs lagged the state by one tick on a transition.** The finish check runs after the
   DRYING branch has already filled in the outputs, so the heater stayed on for one extra
   second and the CSV logged `DRYING` beside a heater that should have been off. Transitions
   now re-derive their outputs in the same tick.

One build-config bug also surfaced: `build_unflags` sat in `[common]`, which `[esp32]`
interpolates `build_flags` from but does not inherit options from — so the framework's
`-std=gnu++11` won and every struct with a default member initializer stopped being an
aggregate. It is repeated in `[esp32]` now.

### Toolchain used

PlatformIO 6.1.19 on Python 3.14, espressif32 platform, MSYS2 UCRT64 GCC 16.1.0 for the
host tests (`C:\msys64\ucrt64\bin` on PATH). The `ESP32Async/ESPAsyncWebServer` +
`ESP32Async/AsyncTCP` dependency resolved without trouble.

---

## Decisions taken where the spec was silent

- **No collector sensor in v1.** The intake-fan rule in ARCHITECTURE §6 wants
  `T_collector_out > T_cabinet + 2 °C`, but the v1 pin map has no collector sensor.
  `TickInputs::collectorValid` is therefore `false` and the intake fans run whenever the
  machine is in PREHEAT/DRYING. The rule is implemented and will start working the moment a
  third sensor is wired — only `main.cpp` changes.
- **CSV download at `/logs/<file>`, not `/api/logs/<file>`.** Serving those files with
  `serveStatic` gives true chunked streaming for free and avoids compiling the async server
  with its regex-routing option. `/api/logs` still lists, `/api/logs/delete` still deletes.
- **POST bodies are form-urlencoded, not JSON.** The page has no framework; `URLSearchParams`
  is one line, and it keeps `AsyncJson` out of the build.
- **Invalid sensors log blank cells, not zeros.** A gap in Excel is honest; a zero is a lie
  the chart will plot.
- **Blind purge limit.** If the *sensors* are what failed, the fault path cannot see the
  temperature, so it runs the stack fan for 10 minutes and then stops rather than forever.
- **`log seed` writes at 60 s spacing** regardless of `logIntervalS`. 24 h at 10 s is 8 640
  rows and LittleFS is slow enough that seeding would take minutes.
- **Fault latch keeps the first cause.** A cascade (over-temp → then the klixon opens) still
  reports over-temp, because that is the one worth investigating.
- **Over-temp reads the unfiltered sensor value.** The control loop uses the median+EMA
  filtered reading; `Safety` uses the raw one. Found by the tests: at alpha 0.3 a jump to
  70 °C took six ticks to cross the 65 °C line, because the filter was smoothing a
  protective limit. The SHT31 driver already CRC-checks and range-checks every read, so a
  value that arrives at all is a real one and does not need smoothing.
- **State transitions re-derive their outputs in the same tick.** Also found by the tests:
  the finish check runs after the DRYING branch has already filled in the outputs, so the
  heater stayed on for one extra second and the CSV logged `DRYING` next to a heater that
  should have been off.

## One thing the model exposed

In the simulated cabinet the mandatory 30 s minimum off-time costs roughly 3 °C of
undershoot: the model has a ~200 s thermal time constant, so it loses about 3 °C before the
heater is allowed back on. A real cabinet loaded with wet food has far more thermal mass and
will sit much tighter. If the real machine turns out to swing more than a couple of degrees,
`min_off` is the knob — it is configurable at runtime (`set minoff 15`, `save`), no reflash.
