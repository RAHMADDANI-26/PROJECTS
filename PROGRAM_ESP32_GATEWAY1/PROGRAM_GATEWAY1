#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_system.h>

// =====================================================
// UART KE GATEWAY 2
// =====================================================

#define RXD2 16
#define TXD2 17

HardwareSerial GatewayUART(2);

// =====================================================
// ESP-NOW CHANNEL
// =====================================================

#define WIFI_CHANNEL 1

// =====================================================
// RESET OTOMATIS SETIAP 7 HARI
// =====================================================

#define RESET_INTERVAL 604800000UL

unsigned long bootTime = 0;

// =====================================================
// UKURAN DATA
// =====================================================

// DIRIS A-20
#define DIRIS_DATA_SIZE 126

// AlertData dari Node Alert
// nodeID[12]       = 12 byte
// powerStatus      = 1 byte
// batteryPercent   = 4 byte
// timestamp        = 4 byte
// TOTAL            = 21 byte
#define ALERT_DATA_SIZE 21

// =====================================================
// BUFFER JSON
// =====================================================

#define JSON_BUFFER_SIZE 2048

// =====================================================
// STRUCT DATA DIRIS A-20
// HARUS SAMA PERSIS DENGAN NODE DIRIS
// =====================================================

#pragma pack(push, 1)

typedef struct {

  uint16_t header;
  uint16_t panel_id;

  uint32_t message_id;
  uint32_t timestamp;

  int32_t v12;
  int32_t v23;
  int32_t v31;

  int32_t v1;
  int32_t v2;
  int32_t v3;

  int32_t freq;

  int32_t i1;
  int32_t i2;
  int32_t i3;
  int32_t i_n;

  int32_t kw_total;
  int32_t kvar_total;
  int32_t kva_total;

  int32_t kw_l1;
  int32_t kw_l2;
  int32_t kw_l3;

  int32_t pf_total;

  int32_t thd_v12;
  int32_t thd_v23;
  int32_t thd_v31;

  int32_t thd_v1;
  int32_t thd_v2;
  int32_t thd_v3;

  int32_t thd_i1;
  int32_t thd_i2;
  int32_t thd_i3;

  int32_t kwh_total;

  uint16_t checksum;

} ModbusData;

#pragma pack(pop)

static_assert(
  sizeof(ModbusData) == DIRIS_DATA_SIZE,
  "ERROR: ModbusData bukan 126 byte!"
);

// =====================================================
// STRUCT ALERT
// =====================================================

#pragma pack(push, 1)

typedef struct {

  char nodeID[12];

  uint8_t powerStatus;

  float batteryPercent;

  uint32_t timestamp;

} AlertData;

#pragma pack(pop)

static_assert(
  sizeof(AlertData) == ALERT_DATA_SIZE,
  "ERROR: AlertData bukan 21 byte!"
);

// =====================================================
// BUFFER ESP-NOW
// =====================================================

uint8_t dirisBuffer[DIRIS_DATA_SIZE];

uint8_t alertBuffer[ALERT_DATA_SIZE];

volatile bool dirisDataReceived = false;
volatile bool alertDataReceived = false;

// =====================================================
// CRC16 MODBUS
// =====================================================

uint16_t calculateCRC16(
  const uint8_t *data,
  size_t length
)
{
  uint16_t crc = 0xFFFF;

  for (size_t i = 0; i < length; i++)
  {
    crc ^= data[i];

    for (uint8_t j = 0; j < 8; j++)
    {
      if (crc & 0x0001)
      {
        crc >>= 1;
        crc ^= 0xA001;
      }
      else
      {
        crc >>= 1;
      }
    }
  }

  return crc;
}

// =====================================================
// CALLBACK ESP-NOW
// =====================================================

void OnDataRecv(
  const esp_now_recv_info_t *info,
  const uint8_t *incomingData,
  int len
)
{
  // ===================================================
  // DATA DIRIS
  // ===================================================

  if (len == DIRIS_DATA_SIZE)
  {
    memcpy(
      dirisBuffer,
      incomingData,
      DIRIS_DATA_SIZE
    );

    dirisDataReceived = true;

    Serial.println();
    Serial.println("========================================");
    Serial.println("ESP-NOW DATA DIRIS DITERIMA");
    Serial.print("Ukuran : ");
    Serial.print(len);
    Serial.println(" byte");
    Serial.println("========================================");
  }

  // ===================================================
  // DATA ALERT
  // ===================================================

  else if (len == ALERT_DATA_SIZE)
  {
    memcpy(
      alertBuffer,
      incomingData,
      ALERT_DATA_SIZE
    );

    alertDataReceived = true;

    Serial.println();
    Serial.println("========================================");
    Serial.println("ESP-NOW ALERT DITERIMA");
    Serial.print("Ukuran : ");
    Serial.print(len);
    Serial.println(" byte");
    Serial.println("========================================");
  }

  // ===================================================
  // DATA TIDAK DIKENAL
  // ===================================================

  else
  {
    Serial.println();
    Serial.println("========================================");
    Serial.println("DATA ESP-NOW TIDAK DIKENAL");
    Serial.print("Ukuran : ");
    Serial.print(len);
    Serial.println(" byte");
    Serial.println("========================================");
  }
}

// =====================================================
// PROSES DATA DIRIS
// =====================================================

void processDirisData()
{
  ModbusData data;

  memcpy(
    &data,
    dirisBuffer,
    sizeof(ModbusData)
  );

  if (data.header != 0xAAAA)
  {
    Serial.println(
      "ERROR: Header DIRIS tidak valid!"
    );

    return;
  }

  uint16_t calculatedCRC =
    calculateCRC16(
      (uint8_t *)&data,
      sizeof(ModbusData) - sizeof(uint16_t)
    );

  if (calculatedCRC != data.checksum)
  {
    Serial.println(
      "ERROR: CRC DIRIS tidak valid!"
    );

    Serial.print(
      "CRC diterima : 0x"
    );

    Serial.println(
      data.checksum,
      HEX
    );

    Serial.print(
      "CRC dihitung  : 0x"
    );

    Serial.println(
      calculatedCRC,
      HEX
    );

    return;
  }

  float v12 = data.v12 / 100.0;
  float v23 = data.v23 / 100.0;
  float v31 = data.v31 / 100.0;

  float v1 = data.v1 / 100.0;
  float v2 = data.v2 / 100.0;
  float v3 = data.v3 / 100.0;

  float freq = data.freq / 100.0;

  float i1 = data.i1 / 1000.0;
  float i2 = data.i2 / 1000.0;
  float i3 = data.i3 / 1000.0;
  float in = data.i_n / 1000.0;

  float kw_total = data.kw_total / 10.0;
  float kvar_total = data.kvar_total / 10.0;
  float kva_total = data.kva_total / 10.0;

  float kw_l1 = data.kw_l1 / 10.0;
  float kw_l2 = data.kw_l2 / 10.0;
  float kw_l3 = data.kw_l3 / 10.0;

  float pf = data.pf_total / 1000.0;

  float thd_v12 = data.thd_v12 / 10.0;
  float thd_v23 = data.thd_v23 / 10.0;
  float thd_v31 = data.thd_v31 / 10.0;

  float thd_v1 = data.thd_v1 / 10.0;
  float thd_v2 = data.thd_v2 / 10.0;
  float thd_v3 = data.thd_v3 / 10.0;

  float thd_i1 = data.thd_i1 / 10.0;
  float thd_i2 = data.thd_i2 / 10.0;
  float thd_i3 = data.thd_i3 / 10.0;

  float energy = data.kwh_total / 10.0;

  char json[JSON_BUFFER_SIZE];

  snprintf(
    json,
    sizeof(json),

    "{"
      "\"device\":\"DIRIS_A20\","
      "\"panel_id\":%u,"
      "\"message_id\":%lu,"
      "\"timestamp\":%lu,"

      "\"voltage\":{"
        "\"v12\":%.2f,"
        "\"v23\":%.2f,"
        "\"v31\":%.2f,"
        "\"v1\":%.2f,"
        "\"v2\":%.2f,"
        "\"v3\":%.2f"
      "},"

      "\"frequency\":%.2f,"

      "\"current\":{"
        "\"i1\":%.3f,"
        "\"i2\":%.3f,"
        "\"i3\":%.3f,"
        "\"in\":%.3f"
      "},"

      "\"power\":{"
        "\"total\":%.1f,"
        "\"kvar\":%.1f,"
        "\"kva\":%.1f,"
        "\"l1\":%.1f,"
        "\"l2\":%.1f,"
        "\"l3\":%.1f"
      "},"

      "\"pf\":%.3f,"

      "\"thd_voltage\":{"
        "\"v12\":%.1f,"
        "\"v23\":%.1f,"
        "\"v31\":%.1f,"
        "\"v1\":%.1f,"
        "\"v2\":%.1f,"
        "\"v3\":%.1f"
      "},"

      "\"thd_current\":{"
        "\"i1\":%.1f,"
        "\"i2\":%.1f,"
        "\"i3\":%.1f"
      "},"

      "\"energy\":%.1f"
    "}",

    data.panel_id,
    (unsigned long)data.message_id,
    (unsigned long)data.timestamp,

    v12,
    v23,
    v31,

    v1,
    v2,
    v3,

    freq,

    i1,
    i2,
    i3,
    in,

    kw_total,
    kvar_total,
    kva_total,

    kw_l1,
    kw_l2,
    kw_l3,

    pf,

    thd_v12,
    thd_v23,
    thd_v31,

    thd_v1,
    thd_v2,
    thd_v3,

    thd_i1,
    thd_i2,
    thd_i3,

    energy
  );

  Serial.println();
  Serial.println("========== DIRIS JSON ==========");
  Serial.println(json);
  Serial.println("================================");

  GatewayUART.println(json);

  Serial.println(
    "DIRIS JSON -> Gateway 2"
  );
}

// =====================================================
// PROSES ALERT
// =====================================================

void processAlertData()
{
  AlertData data;

  memcpy(
    &data,
    alertBuffer,
    sizeof(AlertData)
  );

  data.nodeID[
    sizeof(data.nodeID) - 1
  ] = '\0';

  if (strlen(data.nodeID) == 0)
  {
    Serial.println(
      "ERROR: Node ID kosong!"
    );

    return;
  }

  if (
    data.powerStatus != 0 &&
    data.powerStatus != 1
  )
  {
    Serial.println(
      "ERROR: Status PLN tidak valid!"
    );

    return;
  }

  if (
    data.batteryPercent < 0.0 ||
    data.batteryPercent > 100.0
  )
  {
    Serial.println(
      "WARNING: Persentase baterai di luar range!"
    );
  }

  const char *status;

  if (data.powerStatus == 1)
  {
    status = "ON";
  }
  else
  {
    status = "OFF";
  }

  char json[512];

  snprintf(
    json,
    sizeof(json),

    "{"
      "\"device\":\"POWER_ALERT\","
      "\"node\":\"%s\","
      "\"status\":\"%s\","
      "\"power_status\":%u,"
      "\"battery_percent\":%.1f,"
      "\"timestamp\":%lu"
    "}",

    data.nodeID,
    status,
    data.powerStatus,
    data.batteryPercent,
    (unsigned long)data.timestamp
  );

  Serial.println();
  Serial.println("========== ALERT JSON ==========");
  Serial.println(json);
  Serial.println("================================");

  GatewayUART.println(json);

  Serial.println(
    "ALERT JSON -> Gateway 2"
  );
}

// =====================================================
// RESET OTOMATIS 7 HARI
// =====================================================

void checkWeeklyReset()
{
  if (millis() - bootTime >= RESET_INTERVAL)
  {
    Serial.println();
    Serial.println("========================================");
    Serial.println("RESET OTOMATIS SETELAH 7 HARI");
    Serial.println("Gateway 1 akan restart...");
    Serial.println("========================================");

    delay(1000);

    ESP.restart();
  }
}

// =====================================================
// SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);

  delay(1000);

  // Mulai hitung waktu sejak boot
  bootTime = millis();

  Serial.println();
  Serial.println("========================================");
  Serial.println("       GATEWAY 1 - DIRIS + ALERT");
  Serial.println("========================================");

  GatewayUART.begin(
    115200,
    SERIAL_8N1,
    RXD2,
    TXD2
  );

  Serial.println(
    "UART Gateway 2 aktif"
  );

  Serial.println(
    "RXD2 = GPIO16"
  );

  Serial.println(
    "TXD2 = GPIO17"
  );

  WiFi.mode(WIFI_STA);

  WiFi.disconnect();

  delay(100);

  esp_err_t channelResult =
    esp_wifi_set_channel(
      WIFI_CHANNEL,
      WIFI_SECOND_CHAN_NONE
    );

  if (channelResult == ESP_OK)
  {
    Serial.print(
      "ESP-NOW Channel : "
    );

    Serial.println(
      WIFI_CHANNEL
    );
  }
  else
  {
    Serial.println(
      "ERROR: Gagal mengatur channel!"
    );
  }

  Serial.print(
    "MAC Gateway 1 : "
  );

  Serial.println(
    WiFi.macAddress()
  );

  if (
    esp_now_init() != ESP_OK
  )
  {
    Serial.println(
      "ERROR: ESP-NOW gagal diinisialisasi!"
    );

    while (true)
    {
      delay(1000);
    }
  }

  Serial.println(
    "ESP-NOW berhasil diinisialisasi"
  );

  esp_now_register_recv_cb(
    OnDataRecv
  );

  Serial.println(
    "Callback ESP-NOW aktif"
  );

  Serial.println();
  Serial.println("========================================");
  Serial.println("GATEWAY 1 SIAP");
  Serial.println("========================================");

  Serial.println(
    "Menerima ESP-NOW:"
  );

  Serial.println(
    "- DIRIS A-20 : 126 byte"
  );

  Serial.println(
    "- ALERT      : 21 byte"
  );

  Serial.println();
  Serial.println(
    "Mengirim JSON melalui UART:"
  );

  Serial.println(
    "GPIO17 -> Gateway 2 RX"
  );

  Serial.println(
    "========================================"
  );

  Serial.println(
    "Auto Reset : 7 hari"
  );
}

// =====================================================
// LOOP
// =====================================================

void loop()
{
  // ===================================================
  // CEK RESET 7 HARI
  // ===================================================

  checkWeeklyReset();

  // ===================================================
  // DATA DIRIS
  // ===================================================

  if (dirisDataReceived)
  {
    noInterrupts();

    dirisDataReceived = false;

    interrupts();

    processDirisData();
  }

  // ===================================================
  // DATA ALERT
  // ===================================================

  if (alertDataReceived)
  {
    noInterrupts();

    alertDataReceived = false;

    interrupts();

    processAlertData();
  }

  delay(5);
}