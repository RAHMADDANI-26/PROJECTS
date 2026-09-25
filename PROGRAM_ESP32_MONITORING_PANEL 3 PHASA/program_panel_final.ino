/*
 * ESP32 Modbus Reader + ESP-NOW Sender - DIRIS A-20
 * Data dikirim dalam format BINARY
 */

#include <ModbusMaster.h>
#include <WiFi.h>
#include <esp_now.h>

// ============================================================
// KONFIGURASI MODBUS
// ============================================================
#define MODBUS_TX_PIN    19
#define MODBUS_RX_PIN    18
#define MODBUS_DE_PIN    4
#define MODBUS_DEVICE_ID 5
#define MODBUS_BAUDRATE  9600

// ============================================================
// KONFIGURASI ESP-NOW
// ============================================================
uint8_t gatewayMAC[] = {0x80, 0xF3, 0xDA, 0xAC, 0xF3, 0xF0};  // MAC Gateway

// ============================================================
// DEFINISI ALAMAT REGISTER
// ============================================================
#define REG_V12      50514  // Voltage L1-L2 (V)
#define REG_V23      50516  // Voltage L2-L3 (V)
#define REG_V31      50518  // Voltage L3-L1 (V)
#define REG_V1       50520  // Voltage L1-N (V)
#define REG_V2       50522  // Voltage L2-N (V)
#define REG_V3       50524  // Voltage L3-N (V)
#define REG_FREQ     50526  // Frequency (Hz) U32
#define REG_I1       50528  // Current L1 (A)
#define REG_I2       50530  // Current L2 (A)
#define REG_I3       50532  // Current L3 (A)
#define REG_I_N      50534  // Current Neutral (A)
#define REG_KW_TOTAL 50536 // KW Total (W)
#define REG_KVAR_TOTAL 50538 // KVAR Total (VAr)
#define REG_KVA_TOTAL 50540 // KVA Total (VA)
#define REG_KW_L1    50544 // KW L1 (W)
#define REG_KW_L2    50546 // KW L2 (W)
#define REG_KW_L3    50548 // KW L3 (W)
#define REG_PF_TOTAL 50574 // Power Factor Total

// THD VOLTAGE (51536)
#define REG_THD_V12  51536 // THD U12 (%) U16
#define REG_THD_V23  51537 // THD U23 (%) U16
#define REG_THD_V31  51538 // THD U31 (%) U16
#define REG_THD_V1   51539 // THD V1 (%) U16
#define REG_THD_V2   51540 // THD V2 (%) U16
#define REG_THD_V3   51541 // THD V3 (%) U16

// THD CURRENT (51542)
#define REG_THD_I1   51542 // THD I1 (%) U16
#define REG_THD_I2   51543 // THD I2 (%) U16
#define REG_THD_I3   51544 // THD I3 (%) U16

#define REG_KWH_TOTAL 2836 // Total Energy (kWh)

// ============================================================
// SKALA
// ============================================================
#define SCALE_VOLTAGE    10.0
#define SCALE_FREQ       100.0
#define SCALE_CURRENT    1000.0
#define SCALE_POWER      10.0
#define SCALE_PF         1000.0
#define SCALE_ENERGY     10.0
#define SCALE_THD        10.0

// ============================================================
// OBJEK MODBUS
// ============================================================
ModbusMaster node;

// ============================================================
// STRUCT DATA (BINARY - PACKED)
// ============================================================
#pragma pack(push, 1)
struct ModbusData {
  // Header
  uint16_t header;      // 0xAAAA magic number
  uint16_t panel_id;    // Panel ID
  uint32_t message_id;   // Message counter
  uint32_t timestamp;    // Timestamp (ms)

  // Voltage (x10 = dV)
  int32_t v12;     // dV L1-L2
  int32_t v23;     // dV L2-L3
  int32_t v31;     // dV L3-L1
  int32_t v1;      // dV L1-N
  int32_t v2;      // dV L2-N
  int32_t v3;      // dV L3-N

  // Frequency (x100 = cHz)
  int32_t freq;    // cHz

  // Current (x100 = cA)
  int32_t i1;     // cA L1
  int32_t i2;      // cA L2
  int32_t i3;      // cA L3
  int32_t i_n;     // cA Neutral

  // Power (x10 = dW)
  int32_t kw_total;  // dW Total
  int32_t kvar_total; // dVAr
  int32_t kva_total;  // dVA
  int32_t kw_l1;    // dW L1
  int32_t kw_l2;    // dW L2
  int32_t kw_l3;    // dW L3

  // Power Factor (x1000)
  int32_t pf_total;  // x1000

  // THD Voltage (x10)
  int32_t thd_v12;
  int32_t thd_v23;
  int32_t thd_v31;
  int32_t thd_v1;
  int32_t thd_v2;
  int32_t thd_v3;

  // THD Current (x10)
  int32_t thd_i1;
  int32_t thd_i2;
  int32_t thd_i3;

  // Energy (x10 = dWh)
  int32_t kwh_total;  // dWh

  uint16_t checksum;   // CRC16
};
#pragma pack(pop)

// Ukuran struct: 2+2+4+4 + 6*2 + 2 + 4*2 + 6*2 + 2 + 6*2 + 3*2 + 4 + 2 = 98 bytes

ModbusData modbusData;
uint8_t sendBuffer[sizeof(ModbusData)];

// Counter
uint32_t msgCounter = 0;
uint16_t panelID = 1;

// ============================================================
// STATUS
// ============================================================
bool peerLinked = false;

// ============================================================
// ESP-NOW
// ============================================================
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println("[ESP-NOW] ✅ Data dikirim");
  } else {
    Serial.println("[ESP-NOW] ❌ Gagal kirim");
  }
}

void setupPeer() {
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, gatewayMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (!esp_now_is_peer_exist(gatewayMAC)) {
    esp_now_add_peer(&peerInfo);
  }
  peerLinked = true;
}

void sendViaEspNow() {
  if (!peerLinked) setupPeer();

  // Hitung checksum CRC16
  modbusData.checksum = crc16((uint8_t*)&modbusData, sizeof(ModbusData) - 2);

  // Copy ke buffer
  memcpy(sendBuffer, &modbusData, sizeof(ModbusData));

  Serial.printf("[ESP-NOW] Ukuran data: %d bytes\n", sizeof(ModbusData));

  esp_err_t result = esp_now_send(gatewayMAC, sendBuffer, sizeof(ModbusData));
  if (result != ESP_OK) {
    Serial.printf("[ESP-NOW] ❌ Error: %d\n", result);
  }
}

// ============================================================
// CRC16
// ============================================================
uint16_t crc16(uint8_t *data, uint16_t len) {
  uint16_t crc = 0xFFFF;
  for (uint16_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x0001) {
        crc = (crc >> 1) ^ 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

// ============================================================
// RS485 CONTROL
// ============================================================
void preTransmission() {
  digitalWrite(MODBUS_DE_PIN, HIGH);
  delayMicroseconds(10);
}

void postTransmission() {
  delayMicroseconds(50);
  digitalWrite(MODBUS_DE_PIN, LOW);
}

// ============================================================
// BACA REGISTER U32 (dengan retry)
// ============================================================
uint32_t readU32(uint16_t addr) {
  if (addr == 0) return 0;

  for (int retry = 0; retry < 3; retry++) {
    uint8_t result = node.readHoldingRegisters(addr, 2);
    if (result == node.ku8MBSuccess) {
      uint16_t high = node.getResponseBuffer(0);
      uint16_t low = node.getResponseBuffer(1);
      return ((uint32_t)high << 16) | low;
    }
    delay(100);
  }

  Serial.printf("[ERR] Reg %d gagal\n", addr);
  return 0;
}

// ============================================================
// BACA REGISTER U16 (dengan retry)
// ============================================================
uint16_t readU16(uint16_t addr) {
  if (addr == 0) return 0;

  for (int retry = 0; retry < 3; retry++) {
    uint8_t result = node.readHoldingRegisters(addr, 1);
    if (result == node.ku8MBSuccess) {
      return node.getResponseBuffer(0);
    }
    delay(100);
  }

  Serial.printf("[ERR] Reg %d gagal\n", addr);
  return 0;
}

// ============================================================
// BACA SEMUA DATA
// ============================================================
void readAllData() {
  Serial.println("[M] Membaca data...");

  // Header
  modbusData.header = 0xAAAA;
  modbusData.panel_id = panelID;
  modbusData.message_id = ++msgCounter;
  modbusData.timestamp = millis();

  // VOLTAGE L-L (50514) - simpan raw U32
  modbusData.v12 = (int32_t)(readU32(REG_V12)); delay(100);
  modbusData.v23 = (int32_t)(readU32(REG_V23)); delay(100);
  modbusData.v31 = (int32_t)(readU32(REG_V31)); delay(100);

  // VOLTAGE L-N (50520)
  modbusData.v1 = (int32_t)(readU32(REG_V1)); delay(100);
  modbusData.v2 = (int32_t)(readU32(REG_V2)); delay(100);
  modbusData.v3 = (int32_t)(readU32(REG_V3)); delay(100);

  // FREQUENCY (50526)
  modbusData.freq = (int32_t)(readU32(REG_FREQ)); delay(100);

  // CURRENT (50528)
  modbusData.i1 = (int32_t)(readU32(REG_I1)); delay(100);
  modbusData.i2 = (int32_t)(readU32(REG_I2)); delay(100);
  modbusData.i3 = (int32_t)(readU32(REG_I3)); delay(100);
  modbusData.i_n = (int32_t)(readU32(REG_I_N)); delay(100);

  // POWER (50536)
  modbusData.kw_total = (int32_t)(readU32(REG_KW_TOTAL)); delay(100);
  modbusData.kvar_total = (int32_t)(readU32(REG_KVAR_TOTAL)); delay(100);
  modbusData.kva_total = (int32_t)(readU32(REG_KVA_TOTAL)); delay(100);
  modbusData.kw_l1 = (int32_t)(readU32(REG_KW_L1)); delay(100);
  modbusData.kw_l2 = (int32_t)(readU32(REG_KW_L2)); delay(100);
  modbusData.kw_l3 = (int32_t)(readU32(REG_KW_L3)); delay(100);

  // POWER FACTOR (50574)
  modbusData.pf_total = (int32_t)(readU32(REG_PF_TOTAL)); delay(100);

  // THD VOLTAGE (51536)
  modbusData.thd_v12 = (int32_t)(readU16(REG_THD_V12)); delay(100);
  modbusData.thd_v23 = (int32_t)(readU16(REG_THD_V23)); delay(100);
  modbusData.thd_v31 = (int32_t)(readU16(REG_THD_V31)); delay(100);
  modbusData.thd_v1 = (int32_t)(readU16(REG_THD_V1)); delay(100);
  modbusData.thd_v2 = (int32_t)(readU16(REG_THD_V2)); delay(100);
  modbusData.thd_v3 = (int32_t)(readU16(REG_THD_V3)); delay(100);

  // THD CURRENT (51542)
  modbusData.thd_i1 = (int32_t)(readU16(REG_THD_I1)); delay(100);
  modbusData.thd_i2 = (int32_t)(readU16(REG_THD_I2)); delay(100);
  modbusData.thd_i3 = (int32_t)(readU16(REG_THD_I3)); delay(100);

  // ENERGY (2836)
  modbusData.kwh_total = (int32_t)(readU32(REG_KWH_TOTAL));

  Serial.println("[M] Selesai");
}

// ============================================================
// TAMPILKAN DATA
// ============================================================
void printData() {
  Serial.println("\n========================================");
  Serial.println("📊 DATA DIRIS A-20");
  Serial.println("========================================");

  Serial.printf("\nID: panel=%d, msg=%lu\n", modbusData.panel_id, modbusData.message_id);

  Serial.println("\n⚡ VOLTAGE:");
  Serial.printf("  V12  : %.1f V\n", modbusData.v12 / 100.0);
  Serial.printf("  V23  : %.1f V\n", modbusData.v23 / 100.0);
  Serial.printf("  V31  : %.1f V\n", modbusData.v31 / 100.0);
  Serial.printf("  V1   : %.1f V\n", modbusData.v1 / 100.0);
  Serial.printf("  V2   : %.1f V\n", modbusData.v2 / 100.0);
  Serial.printf("  V3   : %.1f V\n", modbusData.v3 / 100.0);

  Serial.println("\n📈 FREQUENCY:");
  Serial.printf("  Freq : %.2f Hz\n", modbusData.freq / 100.0);

  Serial.println("\n💡 CURRENT:");
  Serial.printf("  I1   : %.3f A\n", modbusData.i1 / 1000.0);
  Serial.printf("  I2   : %.3f A\n", modbusData.i2 / 1000.0);
  Serial.printf("  I3   : %.3f A\n", modbusData.i3 / 1000.0);
  Serial.printf("  I-N  : %.3f A\n", modbusData.i_n / 1000.0);

  Serial.println("\n⚙️ POWER:");
  Serial.printf("  kW Total : %.1f W\n", modbusData.kw_total / 10.0);
  Serial.printf("  kVAR     : %.1f VAr\n", modbusData.kvar_total / 10.0);
  Serial.printf("  kVA      : %.1f VA\n", modbusData.kva_total / 10.0);
  Serial.printf("  kW L1    : %.1f W\n", modbusData.kw_l1 / 10.0);
  Serial.printf("  kW L2    : %.1f W\n", modbusData.kw_l2 / 10.0);
  Serial.printf("  kW L3    : %.1f W\n", modbusData.kw_l3 / 10.0);

  Serial.println("\n📊 POWER FACTOR:");
  Serial.printf("  PF   : %.3f\n", modbusData.pf_total / 1000.0);

  Serial.println("\n📊 THD VOLTAGE:");
  Serial.printf("  V12 : %.1f %%\n", modbusData.thd_v12 / 10.0);
  Serial.printf("  V23 : %.1f %%\n", modbusData.thd_v23 / 10.0);
  Serial.printf("  V31 : %.1f %%\n", modbusData.thd_v31 / 10.0);
  Serial.printf("  V1  : %.1f %%\n", modbusData.thd_v1 / 10.0);
  Serial.printf("  V2  : %.1f %%\n", modbusData.thd_v2 / 10.0);
  Serial.printf("  V3  : %.1f %%\n", modbusData.thd_v3 / 10.0);

  Serial.println("\n📊 THD CURRENT:");
  Serial.printf("  I1  : %.1f %%\n", modbusData.thd_i1 / 10.0);
  Serial.printf("  I2  : %.1f %%\n", modbusData.thd_i2 / 10.0);
  Serial.printf("  I3  : %.1f %%\n", modbusData.thd_i3 / 10.0);

  Serial.println("\n📊 ENERGY:");
  Serial.printf("  kWh  : %.1f kWh\n", modbusData.kwh_total / 10.0);

  Serial.printf("\n📦 Binary size: %d bytes\n", sizeof(ModbusData));
  Serial.println("========================================");
}

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);

  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║  ESP32 Modbus + ESP-NOW (BINARY)║");
  Serial.println("║  Baudrate: 9600 | Slave: 5       ║");
  Serial.println("╚════════════════════════════════════╝");

  pinMode(MODBUS_DE_PIN, OUTPUT);
  digitalWrite(MODBUS_DE_PIN, LOW);

  Serial2.begin(MODBUS_BAUDRATE, SERIAL_8N1, MODBUS_RX_PIN, MODBUS_TX_PIN);

  node.begin(MODBUS_DEVICE_ID, Serial2);
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);

  Serial.printf("[MODBUS] ID: %d, Baudrate: %d\n", MODBUS_DEVICE_ID, MODBUS_BAUDRATE);
  Serial.printf("[INFO] Struct size: %d bytes\n", sizeof(ModbusData));

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() == ESP_OK) {
    Serial.println("[ESP-NOW] ✅ OK");
    esp_now_register_send_cb(OnDataSent);
    setupPeer();
    Serial.printf("[ESP-NOW] MAC: %s\n", WiFi.macAddress().c_str());
  } else {
    Serial.println("[ESP-NOW] ❌ Gagal");
  }

  Serial.println("\n✅ Siap!");
}

// ============================================================
// LOOP
// ============================================================
void loop() {
  static unsigned long lastRead = 0;
  const unsigned long interval = 5000;

  // Restart setiap 7 hari
  static unsigned long bootTime = 0;
  if (bootTime == 0) bootTime = millis();
  const unsigned long restartInterval = 604800000; // 7 hari dalam ms (7 * 24 * 60 * 60 * 1000)
  if (millis() - bootTime >= restartInterval) {
    Serial.println("\n[INFO] Restart ESP32 setiap 7 hari...");
    ESP.restart();
  }

  if (millis() - lastRead >= interval) {
    lastRead = millis();

    readAllData();
    printData();
    sendViaEspNow();
  }
}
