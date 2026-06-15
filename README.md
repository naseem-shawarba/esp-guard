# ESP-Guard

A low-power, battery-friendly security guard built on an **ESP32 DevKitC**. The device spends
almost all of its time in deep sleep and only wakes on an event: the door opening, movement, a button
on a 433 MHz remote, or the timer. When armed and the door opens, it sounds a buzzer and
sends a **Telegram** notification. It can be armed/disarmed and queried with a wireless
remote, configured over a **web portal**, and updated over-the-air — all without a wired
connection.

---

## Table of contents
- [Hardware](#hardware)
- [Wiring](#wiring)
- [How it works](#how-it-works)
- [Power consumption (deep sleep everywhere)](#power-consumption-deep-sleep-everywhere)
- [Web portal (configuration + OTA updates)](#web-portal-configuration--ota-updates)
- [Telegram notifications](#telegram-notifications)
- [Configurable settings](#configurable-settings)
- [Build & flash](#build--flash)
- [Firmware architecture](#firmware-architecture)

---

## Hardware

| Part | Purpose |
|------|---------|
| **ESP32 DevKitC** | The MCU. Deep sleep, WiFi, PWM for LED/buzzer. |
| **Passive piezo buzzer** | Audible alarm (driven with `tone()`). |
| **MC-38 magnetic reed switch** (wired, NC/NO) | Door-open sensor. Used on its **NC** contacts. |
| **Staniot 433 MHz 4-button keyfob remote** | Wireless arm/disarm, status, portal. |
| **433 MHz RF receiver, 4-CH learning-code decoder** (1527/2262) | Decodes the keyfob; drives the ESP32 wake inputs. |
| **Common-cathode RGB LED** + 3 × 220 Ω resistors | Status indication. |
| **10 kΩ resistors** | Pull-downs on the active-high wake inputs. |
| *(optional)* **IR / PIR sensor** | Alternative trigger in place of (or alongside) the reed switch. |

---

## Wiring

### Pin map (defined in `include/config.h`)

| ESP32 GPIO | Connected to | Direction | Notes |
|-----------|--------------|-----------|-------|
| **GPIO 14** | MC-38 door reed switch | input, wake | `wakeupGpioDoor` / `doorPin`, 10 kΩ pull-down |
| **GPIO 27** | RF receiver CH → "toggle" (arm/disarm) | input, wake | `wakeupGpioToggle`, 10 kΩ pull-down |
| **GPIO 4**  | RF receiver CH → "status" | input, wake | `wakeupGpioStatus`, 10 kΩ pull-down |
| **GPIO 13** | RF receiver CH → "portal" (config/OTA) | input, wake | `wakeupGpioPortal` / `portalPin`, 10 kΩ pull-down |
| **GPIO 19** | RGB LED **red** anode (via resistor) | output (PWM) | `redPin`, 220 Ω |
| **GPIO 22** | RGB LED **green** anode (via resistor) | output (PWM) | `greenPin`, 220 Ω |
| **GPIO 23** | RGB LED **blue** anode (via resistor) | output (PWM) | `bluePin`, 220 Ω |
| **GPIO 21** | Piezo buzzer (+) | output (PWM) | `buzzerPin` |

### The key electrical principle: everything wakes on **HIGH**

The ESP32 wakes from sleep using **EXT1 with `ESP_EXT1_WAKEUP_ANY_HIGH`** — i.e. it wakes
when *any* of GPIO 14/27/4/13 goes **HIGH**. So every wake input must:
- **idle LOW** (hence the 10 kΩ pull-downs to GND), and
- be driven **HIGH** when its event happens.

GPIO 14, 27, 4 and 13 are all RTC-capable pins (required for EXT1 wakeup) and none are
boot-strapping pins, so they don't interfere with the ESP32's boot mode.

### Door sensor (MC-38, normally-closed) — GPIO 14

The reed switch and a 10 kΩ pull-down form a simple high/low divider:

```
3.3V ──[ MC-38 reed (NC contacts) ]──┬── GPIO14
                                      │
                                     [10kΩ]
                                      │
                                     GND
```

Using the **NC** contacts of the reed (closed when no magnet is present):
- **Door closed** → magnet is next to the reed → contacts open → GPIO14 pulled **LOW** by the 10 kΩ.
- **Door open** → magnet moves away → contacts close → GPIO14 sees **3.3 V = HIGH** → wakes the ESP32 and reads as "door open".

This matches the firmware, where `digitalRead(doorPin) == HIGH` means *open*.

> **Alternative trigger:** an IR beam-break or PIR sensor can replace the reed switch on
> GPIO 14 instead. Wire it so its output is **HIGH on detection** (and LOW idle); the rest of
> the firmware is unchanged.

### 433 MHz remote + RF receiver — GPIO 27 / 4 / 13

The keyfob's buttons are decoded by the 4-channel 1527/2262 receiver, whose channel outputs
go HIGH while a button is pressed. Set the receiver to **momentary** mode (jumper), and wire
three of its channels to the wake inputs:

| Remote button → RF channel | ESP32 pin | Action |
|---|---|---|
| Button 1 | GPIO 27 (toggle) | Arm / disarm the alarm |
| Button 2 | GPIO 4 (status) | Show current state on the LED |
| Button 3 | GPIO 13 (portal) | Enter the configuration / OTA portal |
| Button 4 | *(spare)* | unused (or wire to GPIO 14 to test the door path) |

The receiver actively drives its outputs LOW when idle, so the 10 kΩ pull-downs are belt-and-
braces (and protect against a floating pin while the receiver powers up).

### RGB LED — GPIO 19 / 22 / 23

A **common-cathode** RGB LED: the common pin to **GND**, and each color leg through its own
current-limiting resistor (220 Ω) to GPIO 19 (R), 22 (G), 23 (B). The firmware drives
them with PWM (`analogWrite`). Colors used:

| Color | Meaning |
|-------|---------|
| **Blue** | Idle / "I'm awake" indicator, also blinks during portal mode |
| **Red** | Armed (status) / counting down the disarm window |
| **Red blink** | counting down the disarm window |
| **Green** | Disarmed (status) |
| **Red ↔ Blue blink** | "Pending" temporary state during a timed armed phase (shown when you press status mid-wait) |
| **Off** | Sleeping, or **stealth mode** |

> If you use a **common-anode** LED instead, the colors will be inverted — tie the common to
> 3.3 V and expect to flip the logic.

### Buzzer — GPIO 21

A **passive piezo** buzzer: one terminal to GPIO 21, the other to GND. The firmware generates
the tone in software with `tone(buzzerPin, frequency, duration)` (default 2 kHz). For louder
output, drive the buzzer through a transistor rather than directly from the GPIO.

---

## How it works

The device is event-driven around deep sleep:

1. **Sleeping** (deep sleep) — waiting for a wake source.
2. **Wake** via EXT1 (door / toggle / status / portal) or the RTC timer, then it acts on the
   cause and goes back to sleep:
   - **Toggle** → arm or disarm. On arming it shows status, then enters a grace period
     (default 30 min) before reporting the door state over Telegram.
   - **Status** → show armed/disarmed on the LED.
   - **Portal** → cancel the alarm and open the web portal (see below).
   - **Door** (only watched while armed) → start a **disarm window** (default 8 s, red LED
     blinking). If you don't disarm in time, it sounds the buzzer, sends a Telegram alert,
     and starts a cooldown.
   - **Timer** → a timed *phase* came due (see below).
3. **Auto temporary-disable** — after a configurable number of triggers (`maxAlarmTriggers`,
   default 3), the alarm temporarily disables itself for a day (configurable) and notifies you,
   then re-arms automatically via the RTC timer.

The multi-minute "armed but waiting" periods (the post-arm grace period and the post-trigger
cooldown) are modelled as timed **phases**: instead of staying awake, the device records the
phase + a deadline in RTC memory and goes back to **deep sleep**, waking on the RTC timer when
the phase is due or earlier on a button. See below.

---

## Power consumption (deep sleep everywhere)

Low power is the whole point of the design, so the device is in **deep sleep** essentially all
the time — including during the long "armed but waiting" periods. There are only two regimes
(figures are typical ESP32 ballparks, not measured on this board):

| State | Rough current | When |
|-------|---------------|------|
| **Deep sleep** | ~10 µA | The default idle state **and** the timed phases (grace / cooldown / temporary-disable). RAM is powered down; only the RTC + EXT1 wake logic stays alive. |
| **Active (WiFi)** | ~80–160 mA | Only briefly: connecting WiFi to send a Telegram message, window to disarm the alarm, or while the web portal is up. |

**How the timed waits stay in deep sleep**

Deep sleep (`esp_deep_sleep_start`) is the lowest power, but it *loses RAM* and restarts from
`setup()` on wake. So state that must survive is kept in **RTC memory** (`RTC_DATA_ATTR`): the
temporary-disable flag, the trigger counter, and the current **phase** (`None` / `Grace` /
`Cooldown` / `TempDisabled`) plus its **deadline**.

Each timed wait is just a phase with a deadline:

1. The action that starts a wait (e.g. arming) records the phase and `deadline = now + duration`,
   then the device deep-sleeps with the **RTC timer** set to the remaining time *and* the
   buttons (toggle/status/portal) still armed as EXT1 wake sources.
2. **Timer wake** → the phase is due, so its follow-up runs (grace → report door state;
   cooldown → maybe escalate to temporary-disable; temporary-disable → re-arm), then the device
   sleeps again.
3. **Button wake** (early) → handle it and recompute the remaining time from the deadline, so an
   interruption doesn't reset the countdown. Pressing **toggle** disarms and cancels the phase;
   **portal** cancels the alarm and opens the portal; **status** shows the pending blink and the
   wait resumes.

The deadline uses a clock that ESP-IDF keeps advancing across deep sleep, so the timing is
accurate even across several button wakes (subject to the RTC oscillator's drift, same as any
deep-sleep timer).

> The only thing that stays awake is the short **8 s disarm window** after the door opens — it's
> brief, time-critical, and blinks the LED, so it runs as a normal awake loop.

---

## Web portal (configuration + OTA updates)

All configuration — WiFi, Telegram, settings, and firmware updates — is done through a built-in
web portal, so a prebuilt `firmware.bin` can be shared and each person sets **their own**
credentials with no rebuild and no secrets baked into the binary.

The portal opens in two ways:
- **Automatically on first run** — if the required credentials are missing, the device boots
  straight into the portal (see below).
- **On demand** — press the **portal** button (GPIO 13) any time, even during a timed armed
  phase (which cancels the alarm first).

When it opens, the device starts a **WiFi Access Point** at `http://10.10.10.1/` and runs a
small web server for a bounded window (default **5 minutes**, configurable). It's a **local
portal**: once you join the AP, the configuration menu **pops up automatically** (the device
answers all DNS and redirects unknown URLs to the menu). The blue LED blinks while it's open;
press the portal button again to exit early, otherwise it closes when the window elapses.

### First-time setup (fresh / unconfigured device)

A freshly-flashed device has no credentials, so on boot it starts a **setup Access Point** with
built-in defaults:

| | |
|---|---|
| **SSID** | `Smart` |
| **Password** | `configure` |

1. Flash `firmware.bin` and power the device.
2. Join the `Smart` WiFi from your phone/laptop — the menu should auto-open (or browse
   to `http://10.10.10.1/`).
3. Open **Configure WiFi & Telegram**, choose **Home (WPA2-PSK)** or **Enterprise**, fill in your
   network + Telegram bot token/chat ID, and save.
4. Power-cycle. The device now connects to your network and runs as an alarm. (You can change the
   setup-AP SSID/password from that same form.)

### Web routes

| URL | Page |
|-----|------|
| `/` | **Menu** — links to the pages below. |
| `/credentials` · `/credentials/save` | **WiFi & Telegram** — network type (PSK/Enterprise), SSID, password/EAP fields, Telegram token + chat ID, setup-AP credentials, optional MAC. Password/token fields are **write-only** (blank = keep current). |
| `/settings` · `/settings/save` | **Settings** — stealth mode + all timing values, in friendly units. Applied immediately (portal-window length applies next session). |
| `/update` | **Firmware update (OTA)** — upload a new `firmware.bin` (served by ESP2SOTA). |

Internally the `web_portal` module owns the AP + web server (+ captive-portal DNS) and composes
the `web_credentials` form, the `web_config` settings form, and the ESP2SOTA `/update` endpoint.

---

## Telegram notifications

The device reports events to a Telegram chat over your WiFi network — either **WPA2-PSK** (home
WiFi) or **WPA2-Enterprise**, selected in the credentials form.

- WiFi is connected on demand (with a connect timeout, so a misconfigured device doesn't hang),
  then messages are sent via `UniversalTelegramBot` over TLS.
- Messages you'll receive:
  - `The Door has been opened` — alarm tripped (after the disarm window expired).
  - `Door is open` / `Door is closed` — door state reported at the end of the arming grace period.
  - `The alarm has been temporarily deactivated` — after too many triggers.
  - `The alarm has been re-activated after temporal deactivation` — auto re-arm.
  - `Exiting portal mode` — left the web portal early.

### Credentials

Credentials live in the ESP32's NVS (flash key-value store), namespace **`credentials`** —
nothing is hard-coded in the firmware. You set them through the portal (above), **not** by
editing source. The fields:

| Key | Required? | Meaning |
|-----|-----------|---------|
| `WIFI_SSID` | **Required** | Your WiFi network name (PSK or enterprise). |
| `WIFI_PASSWORD` | Required for **PSK** | Home WiFi password. |
| `EAP_IDENTITY`, `EAP_USERNAME`, `EAP_PASSWORD` | Required for **Enterprise** | WPA2-Enterprise (PEAP/TTLS) credentials. |
| `BOT_TOKEN` | **Required** | Telegram bot token. |
| `CHAT_ID` | **Required** | Telegram chat ID to notify. |
| `OTA_SSID`, `OTA_PASSWORD` | Optional | Setup-AP credentials; fall back to the built-in defaults (`Smart` / `configure`) when unset. |
| `newMACAddress` (6 bytes) | Optional | MAC address to spoof for the station interface; applied only if set. |

A device is considered **configured** once it has a WiFi SSID, the matching WiFi secret
(PSK password *or* EAP username+password), and both Telegram fields. Until then it boots into
the setup portal automatically.

> NVS is not encrypted by default — fine for a personal project. If you need the stored secrets
> protected against a flash dump, enable ESP32 flash + NVS encryption.

---

## Configurable settings

Editable from the web portal; defaults live in `include/config.h` and are stored in the NVS
`settings` namespace.

| Setting | Default | Description |
|---------|---------|-------------|
| Stealth mode | off | When on, the LED stays dark and the buzzer is silent. |
| Deactivation window | 8 s | Time to disarm after the door opens before the alarm sounds. |
| LED blink interval | 500 ms | Blink rate during the disarm window. |
| Post-activation delay | 30 min | Grace period after arming before reporting door state. |
| Post-trigger delay | 20 min | Cooldown after the alarm sounds. |
| Temporary-disable period | 24 h | How long the alarm stays auto-disabled after too many triggers. |
| Portal window | 5 min | How long the web portal stays open. |

---

## Build & flash

This is a [PlatformIO](https://platformio.org/) project (Espressif32 / Arduino framework,
arduino-esp32 2.0.17).

```bash
# Build
pio run

# Build + flash over USB
pio run -t upload

# Serial monitor (115200 baud)
pio device monitor
```

The compiled application is at `.pio/build/esp32dev/firmware.bin` — this is the file you
upload via the web portal's `/update` page for over-the-air updates.

---

## Firmware architecture

Code is split into focused PlatformIO libraries under `lib/`, each a small namespace of
free functions; `src/main.cpp` just wires them together in `setup()`.

| Module | Responsibility |
|--------|----------------|
| `config.h` (in `include/`) | Pin map, default timings, buzzer settings, setup-AP defaults |
| `rgb_led` | RGB status LED (incl. stealth gating and sleep-safe off via GPIO hold) |
| `buzzer` | Piezo buzzer tones (stealth-aware) |
| `credentials` | Single source for the NVS `credentials` namespace (WiFi PSK/Enterprise, Telegram, setup-AP, optional MAC) + `isConfigured()` |
| `connectivity` | WiFi (PSK or WPA2-Enterprise) + Telegram messaging, with a connect timeout |
| `web_portal` | Temporary SoftAP + web server + captive-portal DNS; hosts the menu and composes the forms + OTA |
| `web_credentials` | The credentials form routes (`/credentials`, `/credentials/save`) |
| `web_config` | The settings form routes (`/settings`, `/settings/save`) |
| `settings` | Runtime-configurable settings, persisted in NVS |
| `alarm` | Alarm state, arm/disarm, status display, and the timed **phase** machine (grace / cooldown / temporary-disable) backed by RTC memory |
| `deep_sleep` | Wake-cause dispatch + phase-aware deep-sleep configuration |
