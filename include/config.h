#pragma once

#include <Arduino.h> // for GPIO_NUM_* and Arduino types

// =========================
// Pin Definitions (GPIO)
// =========================
#define PIN_BITMASK(GPIO) (1ULL << GPIO) // 2 ^ GPIO_NUMBER in hex

// Wakeup pins
#define wakeupGpioDoor GPIO_NUM_14
#define wakeupGpioStatus GPIO_NUM_4 // gpio 13 - 20
#define wakeupGpioToggle GPIO_NUM_27
#define wakeupGpioPortal GPIO_NUM_13

// Input/output pins
#define bluePin 23
#define greenPin 22
#define redPin 19
#define buzzerPin 21
#define doorPin 14
#define portalPin 13
// #define builtInLedPin 2

// =========================
// Setup Access Point (bootstrap; non-secret, same for every device)
// Used when no AP credentials are stored yet, so a freshly-flashed device is reachable.
// =========================
#define DEFAULT_AP_SSID "Smart"
#define DEFAULT_AP_PASSWORD "configure" // must be >= 8 chars for WPA2

// =========================
// Buzzer settings
// =========================
const int buzzerChannel = 0;    // PWM channel 0
const int buzzerFreq = 2000;    // Frequency in Hz
const int buzzerResolution = 8; // 8-bit resolution

// =========================
// Alarm limits
// =========================
const int maxAlarmTriggers = 3;

// =========================
// Default Settings (used as fallbacks by the settings module when NVS is empty)
// =========================
const bool DEFAULT_stealthMode = false;                                             // LED + buzzer enabled
const unsigned long DEFAULT_alarmDeactivationWindowMs = 8000;                       // 8 seconds to deactivate alarm
const unsigned long DEFAULT_alarmDeactivationLedBlinkMs = 500;                      // LED blink interval
const unsigned long DEFAULT_postAlarmActivationDelayMs = 1000UL * 60 * 30;          // 30 minutes after alarm activation
const unsigned long DEFAULT_postAlarmTriggerDelayMs = 1000UL * 60 * 20;             // 20 minutes after alarm activation
const unsigned long DEFAULT_temporarilyDisableAlarmDelayMs = 1000UL * 60 * 60 * 24; // 1 Day
const unsigned long DEFAULT_portalDurationMs = 1000UL * 60 * 5;                      // 5 minutes
