#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include <Preferences.h>

// ===== WIFI Telegram libraries =====
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <esp_wifi.h>
#include <esp_eap_client.h>

// ===== OTA libraries =====
#include <WiFiAP.h>
#include <WebServer.h>
#include <ESP2SOTA.h>

// ===== WIFI CREDENTIALS =====

// =========================
// Global Objects
// =========================
WiFiClientSecure client;
UniversalTelegramBot *bot = nullptr;
Preferences preferences;
WebServer server(80);

// =========================
// Pin Definitions (GPIO)
// =========================
#define PIN_BITMASK(GPIO) (1ULL << GPIO) // 2 ^ GPIO_NUMBER in hex

// Wakeup pins
#define wakeupGpioDoor GPIO_NUM_14
#define wakeupGpioStatus GPIO_NUM_4 // gpio 13 - 20
#define wakeupGpioToggle GPIO_NUM_15
#define wakeupGpioEnableOTA GPIO_NUM_2

// Input/output pins
#define bluePin 23
#define greenPin 22
#define redPin 19
#define buzzerPin 21
#define doorPin 14
#define OTAPin 2
// #define builtInLedPin 2

// Buzzer settings
const int buzzerChannel = 0;    // PWM channel 0
const int buzzerFreq = 2000;    // Frequency in Hz
const int buzzerResolution = 8; // 8-bit resolution

// =========================
// Alarm State
// =========================
bool isAlarmActive = false; // will be restored from NVS
RTC_DATA_ATTR bool temporarilyDisableAlarm = false;
int maxAlarmTriggers = 3;
RTC_DATA_ATTR int alarmTriggerCount = 0;
// =========================
// Timing Constants (ms)
// =========================
const unsigned long alarmDeactivationWindowMs = 8000;                       // 8 seconds to deactivate alarm
const unsigned long alarmDeactivationLedBlinkMs = 500;                      // LED blink interval
const unsigned long postAlarmActivationDelayMs = 1000UL * 60 * 30;          // 30 minutes after alarm activation
const unsigned long postAlarmTriggerDelayMs = 1000UL * 60 * 20;             // 20 minutes after alarm activation
const unsigned long temporarilyDisableAlarmDelayMs = 1000UL * 60 * 60 * 24; // 1 Day
const unsigned long otaModeDurationMs = 1000UL * 60 * 5;                    // 5 minutes
// =========================
// WiFi / Telegram
// =========================
void connectToWiFi()
{
  Serial.println("Connecting to WPA2-Enterprise WiFi...");

  // Fully stop WiFi
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);

  // Set mode again (initializes but not connected yet)
  WiFi.mode(WIFI_STA);

  preferences.begin("credentials", true);
  uint8_t newMACAddress[6];
  size_t bytesRead = preferences.getBytes("newMACAddress", newMACAddress, 6);
  preferences.end();


  // Change ESP32 Mac Address
  esp_err_t err = esp_wifi_set_mac(WIFI_IF_STA, newMACAddress);
  if (err == ESP_OK)
  {
    Serial.println("Success changing MAC Address");
  }
  else
  {
    Serial.printf("Failed to set MAC: %d\n", err);
  }

  preferences.begin("credentials", true);
  String WIFI_SSID = preferences.getString("WIFI_SSID", "");
  String EAP_IDENTITY = preferences.getString("EAP_IDENTITY", "");
  String EAP_USERNAME = preferences.getString("EAP_USERNAME", "");
  String EAP_PASSWORD = preferences.getString("EAP_PASSWORD", "");

  preferences.end();

  WiFi.begin(WIFI_SSID.c_str());

  // Enterprise credentials
  esp_eap_client_set_identity((uint8_t *)EAP_IDENTITY.c_str(), EAP_IDENTITY.length());
  esp_eap_client_set_username((uint8_t *)EAP_USERNAME.c_str(), EAP_USERNAME.length());
  esp_eap_client_set_password((uint8_t *)EAP_PASSWORD.c_str(), EAP_PASSWORD.length());
  esp_wifi_sta_enterprise_enable();

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Required for HTTPS Telegram connection
  client.setInsecure();
}

bool sendMessageToChat(const String &message)
{
  delay(1000);
  preferences.begin("credentials", true);
  String CHAT_ID = preferences.getString("CHAT_ID", "");
  String BOT_TOKEN = preferences.getString("BOT_TOKEN", "");
  preferences.end();
  delay(500);
  bot = new UniversalTelegramBot(BOT_TOKEN.c_str(), client);
  Serial.print("Sending message: ");
  Serial.println(message);

  bool sent = bot->sendMessage(CHAT_ID, message, "");
  Serial.println(sent ? "Message sent successfully" : "Message failed");
  delay(1000);
  return sent;
}

// =========================
// Alarm Functions
// =========================
void beepBuzzer(int durationMs = 500, int frequency = 2000)
{
  tone(buzzerPin, frequency, durationMs);
  delay(durationMs);
}

void showStatus()
{
  Serial.println(isAlarmActive ? "Alarm is active" : "Alarm is not active");
  if (isAlarmActive)
  {
    ledSetRed();
  }
  else
  {
    ledSetGreen();
  }

  delay(4000);
  ledSetBlue();
}

void loadAlarmState()
{
  preferences.begin("settings", true); // read-only
  isAlarmActive = preferences.getBool("isAlarmActive", false);
  preferences.end();
}

void saveAlarmState()
{
  preferences.begin("settings", false);
  preferences.putBool("isAlarmActive", isAlarmActive);
  preferences.end();
}

void toggleAlarm()
{
  isAlarmActive = !isAlarmActive;
  saveAlarmState();

  Serial.println(isAlarmActive ? "Activated Alarm" : "Deactivated Alarm");
  showStatus();

  if (isAlarmActive)
  {
    delay(postAlarmActivationDelayMs);
    bool doorState = digitalRead(doorPin);
    const char *message = doorState ? "Door is open" : "Door is closed";

    connectToWiFi();
    sendMessageToChat(message);
  }
}

void temporarilyEnableOTA()
{
  delay(1000);
  preferences.begin("credentials", true);
  String OTA_SSID = preferences.getString("OTA_SSID", "");
  String OTA_PASSWORD = preferences.getString("OTA_PASSWORD", "");
  preferences.end();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(OTA_SSID.c_str(), OTA_PASSWORD.c_str());
  delay(1000);
  IPAddress IP = IPAddress(10, 10, 10, 1);
  IPAddress NMask = IPAddress(255, 255, 255, 0);
  WiFi.softAPConfig(IP, IP, NMask);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  /* INITIALIZE ESP2SOTA LIBRARY */
  ESP2SOTA.begin(&server);
  server.begin();

  delay(1000);
  unsigned long startTime = millis();

  while (millis() - startTime < otaModeDurationMs)
  {
    if ((millis() % 1500 > 750))
    {
      ledSetBlue();
    }
    else
    {
      ledOff();
    }
    if (digitalRead(OTAPin))
    {
      connectToWiFi();
      sendMessageToChat("Exiting OTA Mode");
      break;
    }
    server.handleClient();
    delay(5);
  }
  ledSetBlue();

  Serial.println("OTA window ended");
}

bool waitForAlarmDeactivation()
{
  int elapsed = 0;
  bool ledState = false;

  Serial.println("You have 30 seconds to deactivate the alarm...");

  while (elapsed < alarmDeactivationWindowMs)
  {
    // Blink red LED
    ledState = !ledState;
    if (
        ledState)
    {
      ledSetRed();
    }
    else
    {
      ledOff();
    }

    // Check if toggle button pressed
    if (digitalRead(wakeupGpioToggle) == HIGH)
    {
      toggleAlarm(); // Deactivate alarm
      ledOff();
      Serial.println("Alarm deactivated before it could sound!");
      return true;
    }

    delay(alarmDeactivationLedBlinkMs);
    elapsed += alarmDeactivationLedBlinkMs;
    Serial.println(max(0UL, (alarmDeactivationWindowMs - elapsed) / 1000));
  }

  ledSetBlue();
  return false; // Alarm still active
}

// =========================
// Wakeup Handling
// =========================
void handleExtWakeUp()
{
  uint64_t wakeupPinStatus = esp_sleep_get_ext1_wakeup_status();

  if (wakeupPinStatus & PIN_BITMASK(wakeupGpioDoor))
  {
    Serial.println("Door sensor triggered");

    if (isAlarmActive)
    {
      bool deactivated = waitForAlarmDeactivation();
      if (!deactivated)
      {
        // Alarm still active -> notify and sound buzzer
        alarmTriggerCount += 1;
        connectToWiFi();
        sendMessageToChat("The Door has been opened");
        beepBuzzer(5000, 2500);
        delay(postAlarmTriggerDelayMs);
      }
      if (alarmTriggerCount >= maxAlarmTriggers)
      {
        sendMessageToChat("The alarm has been temporarily deactivated");
        temporarilyDisableAlarm = true;
      }
    }
  }

  if (wakeupPinStatus & PIN_BITMASK(wakeupGpioEnableOTA))
  {
    temporarilyEnableOTA();
  }

  if (wakeupPinStatus & PIN_BITMASK(wakeupGpioToggle))
  {
    toggleAlarm();
  }

  if (wakeupPinStatus & PIN_BITMASK(wakeupGpioStatus))
  {
    showStatus();
  }
}

void handleTimerWakeUp()
{
  temporarilyDisableAlarm = false;
  alarmTriggerCount = 0;
  sendMessageToChat("The alarm has been re-activated after temporal deactivation");
}

void handleWakeUpReason()
{
  esp_sleep_wakeup_cause_t wakeupReason = esp_sleep_get_wakeup_cause();

  switch (wakeupReason)
  {
  case ESP_SLEEP_WAKEUP_EXT0:
    Serial.println("Wakeup caused by external signal using RTC_IO");
    break;
  case ESP_SLEEP_WAKEUP_EXT1:
    Serial.println("Wakeup caused by external signal using RTC_CNTL");
    handleExtWakeUp();
    break;
  case ESP_SLEEP_WAKEUP_TIMER:
    Serial.println("Wakeup caused by timer");
    handleTimerWakeUp();
    break;
  case ESP_SLEEP_WAKEUP_TOUCHPAD:
    Serial.println("Wakeup caused by touchpad");
    break;
  case ESP_SLEEP_WAKEUP_ULP:
    Serial.println("Wakeup caused by ULP program");
    break;
  default:
    Serial.printf("Wakeup not caused by deep sleep: %d\n", wakeupReason);
    break;
  }
}

// =========================
// Deep Sleep
// =========================
void goToSleep()
{
  // Start with toggle and status buttons (always wake up on these)
  uint64_t wakeupBitmask = PIN_BITMASK(wakeupGpioToggle) | PIN_BITMASK(wakeupGpioStatus) | PIN_BITMASK(wakeupGpioEnableOTA);

  // Include door pin only if alarm is active
  if (isAlarmActive && !temporarilyDisableAlarm)
  {
    wakeupBitmask |= PIN_BITMASK(wakeupGpioDoor);
  }

  // Enable EXT1 wakeup
  esp_sleep_enable_ext1_wakeup_io(wakeupBitmask, ESP_EXT1_WAKEUP_ANY_HIGH);

  // Configure pull-ups / pull-downs
  rtc_gpio_pullup_dis(wakeupGpioDoor);
  rtc_gpio_pulldown_dis(wakeupGpioDoor);

  if (isAlarmActive && !temporarilyDisableAlarm)
  {
    rtc_gpio_pullup_dis(wakeupGpioToggle);
    rtc_gpio_pulldown_dis(wakeupGpioToggle);
  }

  rtc_gpio_pullup_dis(wakeupGpioStatus);
  rtc_gpio_pulldown_dis(wakeupGpioStatus);

  rtc_gpio_pullup_dis(wakeupGpioEnableOTA);
  rtc_gpio_pulldown_dis(wakeupGpioEnableOTA);

  if (temporarilyDisableAlarm)
  {
    esp_sleep_enable_timer_wakeup(temporarilyDisableAlarmDelayMs * 1000ULL);
  }

  Serial.println("Going to sleep now");
  esp_deep_sleep_start();
}

void setColor(int red, int green, int blue)
{
  analogWrite(redPin, red);
  analogWrite(greenPin, green);
  analogWrite(bluePin, blue);
}

void ledOff()
{
  setColor(0, 0, 0);
}

void ledSetRed()
{
  setColor(255, 0, 0);
}

void ledSetGreen()
{
  setColor(0, 255, 0);
}

void ledSetBlue()
{
  setColor(0, 0, 255);
}

void setupRGBLed()
{
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
}

// =========================
// Setup and Loop
// =========================
void setup()
{
  Serial.begin(115200);
  delay(1000);

  setupRGBLed();
  // Initialize pins
  pinMode(doorPin, INPUT);
  pinMode(OTAPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  // Startup indicator
  ledSetBlue();

  // Restore alarm state from NVS
  loadAlarmState();

  handleWakeUpReason();

  ledOff();
  goToSleep();
}

void loop()
{
  // Not used; device sleeps and wakes on interrupts
}