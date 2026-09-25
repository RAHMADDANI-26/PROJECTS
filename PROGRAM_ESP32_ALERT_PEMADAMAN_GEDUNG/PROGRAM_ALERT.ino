/**
 * Node Alert Gedung - ESP-NOW Sensor
 *
 * Fungsi:
 * - Monitor status PLN (ON/OFF) via GPIO
 * - Kirim alert via ESP-NOW ke Gateway
 * - Deep sleep untuk hemat baterai
 * - Reset otomatis setiap 7 hari
 *
 * Hardware: ESP32 (baterai)
 */

// =====================================================
// LIBRARY
// =====================================================

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>

// =====================================================
// IDENTITAS NODE
// =====================================================

#define NODE_ID "GEDUNG_A"


// =====================================================
// PIN
// =====================================================

// GPIO4:
// LOW  = PLN ON
// HIGH = PLN OFF

#define RELAY_DETECT_PIN 4

// Battery ADC
#define BATTERY_PIN 32


// =====================================================
// ESP-NOW
// =====================================================

#define WIFI_CHANNEL 1

// MAC Address Gateway (sesuaikan dengan gateway Anda)
uint8_t gatewayMAC[] = {
  0x80, 0xF3, 0xDA, 0xAC, 0xF3, 0xF0
};


// =====================================================
// DELAY
// =====================================================

// Status harus stabil 1 detik
#define STATUS_DELAY_MS 1000UL

// Waktu menunggu callback ESP-NOW
#define SEND_WAIT_MS 300UL

// Jika gagal kirim, bangun lagi setelah 30 detik
#define RETRY_SLEEP_SECONDS 30ULL

// Backup wake-up setiap 5 menit
#define BACKUP_SLEEP_SECONDS 300ULL


// =====================================================
// RESET OTOMATIS 7 HARI
// =====================================================

#define WEEKLY_RESET_SECONDS 604800ULL

RTC_DATA_ATTR uint64_t totalSleepSeconds = 0;


// =====================================================
// DATA ALERT
// =====================================================

typedef struct __attribute__((packed))
{
  char nodeID[12];
  uint8_t powerStatus;
  float batteryPercent;
  uint32_t timestamp;
} AlertData;

static_assert(
  sizeof(AlertData) == 21,
  "ERROR: AlertData harus 21 byte!"
);


// =====================================================
// STATUS TERAKHIR
// =====================================================

RTC_DATA_ATTR uint8_t lastSentStatus = 255;


// =====================================================
// STATUS ESP-NOW
// =====================================================

static volatile bool sendFinished = false;
static volatile bool sendSucceeded = false;


// =====================================================
// BACA STATUS PLN
// =====================================================

uint8_t readPowerStatus()
{
  int pinState = digitalRead(RELAY_DETECT_PIN);

  // LOW = PLN ON, HIGH = PLN OFF
  return (pinState == LOW) ? 1 : 0;
}


// =====================================================
// BACA BATTERY
// =====================================================

float readBatteryPercent()
{
  long totalADC = 0;

  // Ambil 20 sampel
  for (int i = 0; i < 20; i++)
  {
    totalADC += analogRead(BATTERY_PIN);
    delayMicroseconds(100);
  }

  float adcAverage = totalADC / 20.0f;

  // ADC 12 bit
  float adcVoltage = adcAverage * (3.3f / 4095.0f);

  // Divider 10k + 10k
  float batteryVoltage = adcVoltage * 2.0f;

  // Konversi persentase
  float percent;

  if (batteryVoltage >= 4.20f) percent = 100.0f;
  else if (batteryVoltage >= 4.10f) percent = 90.0f + (batteryVoltage - 4.10f) * 100.0f;
  else if (batteryVoltage >= 4.00f) percent = 80.0f + (batteryVoltage - 4.00f) * 100.0f;
  else if (batteryVoltage >= 3.90f) percent = 60.0f + (batteryVoltage - 3.90f) * 200.0f;
  else if (batteryVoltage >= 3.80f) percent = 40.0f + (batteryVoltage - 3.80f) * 200.0f;
  else if (batteryVoltage >= 3.70f) percent = 20.0f + (batteryVoltage - 3.70f) * 200.0f;
  else if (batteryVoltage >= 3.50f) percent = 5.0f + (batteryVoltage - 3.50f) * 75.0f;
  else percent = 0.0f;

  percent = constrain(percent, 0.0f, 100.0f);

  Serial.printf("[BATTERY] %.3f V = %.1f%%\n", batteryVoltage, percent);

  return percent;
}


// =====================================================
// CALLBACK ESP-NOW (FORMAT KOMPATIBEL)
// =====================================================

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  (void)mac_addr;  // Abaikan warning unused parameter
  sendSucceeded = (status == ESP_NOW_SEND_SUCCESS);
  sendFinished = true;
}


// =====================================================
// INIT WIFI (CEPAT)
// =====================================================

bool initWiFiESPNow()
{
  // Matikan WiFi dulu
  WiFi.disconnect(false, true);
  WiFi.mode(WIFI_STA);

  // Set channel
  if (esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK)
  {
    Serial.println("[WIFI] Gagal set channel");
    return false;
  }

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK)
  {
    Serial.println("[ESP-NOW] Init gagal");
    return false;
  }

  return true;
}


// =====================================================
// KIRIM ALERT
// =====================================================

bool sendAlert(uint8_t powerStatus)
{
  Serial.println();
  Serial.println("========== KIRIM ALERT ==========");

  // Baca battery
  float battery = readBatteryPercent();

  // Init WiFi + ESP-NOW
  if (!initWiFiESPNow())
  {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return false;
  }

  // Register callback
  esp_now_register_send_cb(onDataSent);

  // Tambah peer
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, gatewayMAC, 6);
  peerInfo.channel = WIFI_CHANNEL;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK)
  {
    Serial.println("[ESP-NOW] Gagal tambah peer");
    esp_now_deinit();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return false;
  }

  // Buat packet
  AlertData packet = {};
  strncpy(packet.nodeID, NODE_ID, sizeof(packet.nodeID) - 1);
  packet.powerStatus = powerStatus;
  packet.batteryPercent = battery;
  packet.timestamp = millis();

  // Reset callback status
  sendFinished = false;
  sendSucceeded = false;

  // Kirim
  esp_err_t result = esp_now_send(
    gatewayMAC,
    (const uint8_t*)&packet,
    sizeof(packet)
  );

  if (result != ESP_OK)
  {
    Serial.printf("[ESP-NOW] Send error = %d\n", result);
    esp_now_deinit();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return false;
  }

  // Tunggu callback
  unsigned long start = millis();
  while (!sendFinished && (millis() - start < SEND_WAIT_MS))
  {
    delay(5);
  }

  bool success = sendFinished && sendSucceeded;

  // Print hasil
  Serial.printf("[ALERT] Node=%s | PLN=%s | Battery=%.1f%% | %s\n",
    packet.nodeID,
    powerStatus == 1 ? "ON" : "OFF",
    packet.batteryPercent,
    success ? "TERKIRIM" : "GAGAL"
  );

  // Cleanup
  esp_now_deinit();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(10);

  Serial.println("=================================");

  return success;
}


// =====================================================
// CEK RESET 7 HARI
// =====================================================

bool checkWeeklyReset()
{
  Serial.println();
  Serial.println("========== TIMER RESET ==========");
  Serial.printf("[RESET] Total: %llu detik / %llu detik target\n",
    totalSleepSeconds, WEEKLY_RESET_SECONDS);

  if (totalSleepSeconds >= WEEKLY_RESET_SECONDS)
  {
    Serial.println("[RESET] 7 HARI TERCAPAI - Restart...");
    Serial.flush();
    delay(50);
    totalSleepSeconds = 0;
    ESP.restart();
    return true;
  }
  return false;
}


// =====================================================
// MASUK DEEP SLEEP
// =====================================================

void enterDeepSleep(uint8_t currentStatus, bool retrySoon)
{
  Serial.println();
  Serial.println("=================================");
  Serial.println("       MASUK DEEP SLEEP");
  Serial.println("=================================");

  // Tentukan waktu sleep
  uint64_t sleepSeconds = retrySoon ? RETRY_SLEEP_SECONDS : BACKUP_SLEEP_SECONDS;

  // Tambah waktu total
  totalSleepSeconds += sleepSeconds;

  Serial.printf("[RESET] Sleep: %llu detik, Total: %llu detik\n",
    sleepSeconds, totalSleepSeconds);

  // Cek reset sebelum sleep
  if (totalSleepSeconds >= WEEKLY_RESET_SECONDS)
  {
    Serial.println("[RESET] 7 hari tercapai - Restart...");
    Serial.flush();
    delay(50);
    totalSleepSeconds = 0;
    ESP.restart();
    return;
  }

  // Tentukan wake level
  // Status ON (GPIO LOW) -> Wake saat HIGH (PLN OFF)
  // Status OFF (GPIO HIGH) -> Wake saat LOW (PLN ON)
  gpio_num_t wakeGpio = GPIO_NUM_4;
  int wakeLevel = (currentStatus == 1) ? HIGH : LOW;

  // Setup GPIO untuk wakeup
  rtc_gpio_init(wakeGpio);
  rtc_gpio_set_direction(wakeGpio, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pullup_en(wakeGpio);
  rtc_gpio_pulldown_dis(wakeGpio);

  // Enable EXT0 wakeup
  esp_sleep_enable_ext0_wakeup(wakeGpio, wakeLevel);

  // Enable timer wakeup
  esp_sleep_enable_timer_wakeup(sleepSeconds * 1000000ULL);

  // Info
  Serial.printf("[SLEEP] Status: %s, Wake GPIO4: %d, Timer: %llu detik\n",
    currentStatus == 1 ? "PLN ON" : "PLN OFF",
    wakeLevel,
    sleepSeconds
  );

  // Deep sleep
  Serial.flush();
  delay(10);
  esp_deep_sleep_start();
}


// =====================================================
// SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);
  delay(10);

  Serial.println();
  Serial.println("========================================");
  Serial.println("       NODE ALERT - ESP-NOW SENSOR");
  Serial.println("========================================");
  Serial.printf("Node ID: %s\n", NODE_ID);

  // Cek reset mingguan
  if (checkWeeklyReset()) return;

  // RTC GPIO ke digital
  rtc_gpio_deinit(GPIO_NUM_4);

  // Setup GPIO
  pinMode(RELAY_DETECT_PIN, INPUT_PULLUP);

  // Setup ADC
  analogReadResolution(12);
  analogSetPinAttenuation(BATTERY_PIN, ADC_11db);

  delay(10);

  // Baca status pertama
  uint8_t firstStatus = readPowerStatus();
  Serial.printf("[RELAY] Baca 1: %s\n", firstStatus == 1 ? "PLN ON" : "PLN OFF");

  // Debounce
  delay(STATUS_DELAY_MS);

  uint8_t stableStatus = readPowerStatus();
  Serial.printf("[RELAY] Baca 2: %s\n", stableStatus == 1 ? "PLN ON" : "PLN OFF");

  // Cek stabilitas
  if (firstStatus != stableStatus)
  {
    Serial.println("[RELAY] STATUS TIDAK STABIL - Retry 30 detik");
    enterDeepSleep(stableStatus, true);
    return;
  }

  Serial.printf("[RELAY] Status stabil: %s\n", stableStatus == 1 ? "PLN ON" : "PLN OFF");

  // Kirim jika berubah
  bool retrySoon = false;

  if (stableStatus != lastSentStatus)
  {
    Serial.println("[ALERT] Status berubah - Kirim alert...");

    if (sendAlert(stableStatus))
    {
      lastSentStatus = stableStatus;
      Serial.println("[ALERT] Berhasil disimpan");
    }
    else
    {
      Serial.println("[ALERT] Gagal - Retry 30 detik");
      retrySoon = true;
    }
  }
  else
  {
    Serial.println("[ALERT] Tidak ada perubahan");
  }

  // Deep sleep
  enterDeepSleep(stableStatus, retrySoon);
}


// =====================================================
// LOOP
// =====================================================

void loop()
{
  // Seharusnya tidak pernah jalan
  Serial.println("[ERROR] LOOP TERJALAN!");
  delay(100);
  ESP.restart();
}
