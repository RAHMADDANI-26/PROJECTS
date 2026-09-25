#include <Arduino.h>
#include <WiFi.h>
#include <ETH.h>
#include <Network.h>
#include <PubSubClient.h>
#include <esp_system.h>

// =====================================================
// UART DARI GATEWAY 1
// =====================================================

#define RXD2 5
#define TXD2 17

HardwareSerial GatewayUART(2);

// =====================================================
// WIFI BACKUP
// =====================================================

const char* WIFI_SSID     = "SSID";
const char* WIFI_PASSWORD = "PASS";

// =====================================================
// MQTT
// =====================================================

const char* MQTT_SERVER = "BROKER";
const int   MQTT_PORT   = 1883;

const char* MQTT_USERNAME = "USER";
const char* MQTT_PASSWORD = "PASS";

// =====================================================
// MQTT TOPIC
// =====================================================

const char* MQTT_TOPIC_DIRIS = "power meter/data";

const char* MQTT_TOPIC_ALERT = "alert pemadaman/gedung";

// =====================================================
// NETWORK CLIENT
// =====================================================

NetworkClient netClient;

PubSubClient mqtt(netClient);

// =====================================================
// STATUS NETWORK
// =====================================================

volatile bool ethernetReady = false;

bool wifiReady = false;

bool lastUsingEthernet = false;

// =====================================================
// TIMER
// =====================================================

unsigned long lastMQTTAttempt = 0;
unsigned long lastWiFiCheck = 0;

// =====================================================
// RESET OTOMATIS SETIAP 7 HARI
// =====================================================

#define RESET_INTERVAL 604800000UL

unsigned long bootTime = 0;

// =====================================================
// UART BUFFER
// =====================================================

#define UART_BUFFER_SIZE 4096

String serialBuffer = "";

// =====================================================
// MQTT BUFFER
// =====================================================

#define MQTT_BUFFER_SIZE 4096

// =====================================================
// PENDING DATA
// =====================================================

String pendingJSON = "";
String pendingTopic = "";

bool hasPendingJSON = false;

// =====================================================
// EVENT ETHERNET
// =====================================================

void onNetworkEvent(arduino_event_id_t event)
{
  switch (event)
  {
    case ARDUINO_EVENT_ETH_START:

      Serial.println("[ETH] Started");

      ETH.setHostname(
        "gateway-gedung"
      );

      break;

    case ARDUINO_EVENT_ETH_CONNECTED:

      Serial.println(
        "[ETH] Cable connected"
      );

      break;

    case ARDUINO_EVENT_ETH_GOT_IP:

      Serial.println(
        "[ETH] Got IP"
      );

      Serial.print(
        "[ETH] IP: "
      );

      Serial.println(
        ETH.localIP()
      );

      ethernetReady = true;

      break;

    case ARDUINO_EVENT_ETH_LOST_IP:

      Serial.println(
        "[ETH] Lost IP"
      );

      ethernetReady = false;

      break;

    case ARDUINO_EVENT_ETH_DISCONNECTED:

      Serial.println(
        "[ETH] Disconnected"
      );

      ethernetReady = false;

      break;

    case ARDUINO_EVENT_ETH_STOP:

      Serial.println(
        "[ETH] Stopped"
      );

      ethernetReady = false;

      break;

    default:

      break;
  }
}

// =====================================================
// START WIFI
// =====================================================

void startWiFi()
{
  Serial.println(
    "[WIFI] Menghubungkan..."
  );

  WiFi.mode(
    WIFI_STA
  );

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );
}

// =====================================================
// CEK WIFI
// =====================================================

void checkWiFi()
{
  if (
    millis() - lastWiFiCheck < 1000
  )
  {
    return;
  }

  lastWiFiCheck = millis();

  if (
    WiFi.status() == WL_CONNECTED
  )
  {
    if (!wifiReady)
    {
      Serial.println(
        "[WIFI] Connected"
      );

      Serial.print(
        "[WIFI] IP: "
      );

      Serial.println(
        WiFi.localIP()
      );
    }

    wifiReady = true;
  }
  else
  {
    wifiReady = false;
  }
}

// =====================================================
// PILIH NETWORK
// =====================================================

void selectNetwork()
{
  if (ethernetReady)
  {
    if (!lastUsingEthernet)
    {
      Serial.println();
      Serial.println(
        "[NETWORK] Menggunakan ETHERNET"
      );

      Network.setDefaultInterface(
        ETH
      );

      if (
        mqtt.connected()
      )
      {
        mqtt.disconnect();
      }

      lastUsingEthernet = true;
    }

    return;
  }

  if (wifiReady)
  {
    if (lastUsingEthernet)
    {
      Serial.println();

      Serial.println(
        "[NETWORK] Ethernet tidak tersedia"
      );

      Serial.println(
        "[NETWORK] Beralih ke WIFI"
      );

      Network.setDefaultInterface(
        WiFi.STA
      );

      if (
        mqtt.connected()
      )
      {
        mqtt.disconnect();
      }

      lastUsingEthernet = false;
    }

    return;
  }
}

// =====================================================
// MQTT CONNECT
// =====================================================

void connectMQTT()
{
  if (
    mqtt.connected()
  )
  {
    return;
  }

  if (
    millis() - lastMQTTAttempt < 5000
  )
  {
    return;
  }

  lastMQTTAttempt = millis();

  if (
    !ethernetReady &&
    !wifiReady
  )
  {
    return;
  }

  Serial.println();
  Serial.println(
    "[MQTT] Connecting..."
  );

  String clientID =
    "Gateway2-" +
    String(
      (uint32_t)ESP.getEfuseMac(),
      HEX
    );

  Serial.print(
    "[MQTT] Client ID : "
  );

  Serial.println(
    clientID
  );

  bool result =
    mqtt.connect(
      clientID.c_str(),
      MQTT_USERNAME,
      MQTT_PASSWORD
    );

  if (result)
  {
    Serial.println(
      "[MQTT] Connected"
    );

    Serial.print(
      "[MQTT] Server : "
    );

    Serial.println(
      MQTT_SERVER
    );

    if (
      hasPendingJSON &&
      pendingJSON.length() > 0 &&
      pendingTopic.length() > 0
    )
    {
      Serial.println();

      Serial.println(
        "[MQTT] Mengirim data pending..."
      );

      bool resultPending =
        mqtt.publish(
          pendingTopic.c_str(),
          pendingJSON.c_str(),
          true
        );

      if (resultPending)
      {
        Serial.println(
          "[MQTT] Data pending berhasil dikirim"
        );

        pendingJSON = "";
        pendingTopic = "";
        hasPendingJSON = false;
      }
      else
      {
        Serial.println(
          "[MQTT] Data pending gagal dikirim"
        );
      }
    }
  }
  else
  {
    Serial.print(
      "[MQTT] Gagal connect. State = "
    );

    Serial.println(
      mqtt.state()
    );
  }
}

// =====================================================
// VALIDASI JSON
// =====================================================

bool isValidJSON(
  const String &json
)
{
  if (
    json.length() < 2
  )
  {
    return false;
  }

  if (
    json[0] != '{'
  )
  {
    return false;
  }

  if (
    json[json.length() - 1] != '}'
  )
  {
    return false;
  }

  return true;
}

// =====================================================
// TENTUKAN TOPIC MQTT
// =====================================================

const char* getMQTTTopic(
  const String &json
)
{
  if (
    json.indexOf(
      "\"device\":\"DIRIS_A20\""
    ) >= 0
  )
  {
    return MQTT_TOPIC_DIRIS;
  }

  if (
    json.indexOf(
      "\"device\":\"POWER_ALERT\""
    ) >= 0
  )
  {
    return MQTT_TOPIC_ALERT;
  }

  return nullptr;
}

// =====================================================
// TAMPIL DATA
// =====================================================

void printJSONInfo(
  const String &json,
  const char* topic
)
{
  Serial.println();

  Serial.println(
    "=========================================="
  );

  Serial.println(
    "        DATA DARI GATEWAY 1"
  );

  Serial.println(
    "=========================================="
  );

  Serial.print(
    "JSON Size : "
  );

  Serial.print(
    json.length()
  );

  Serial.println(
    " byte"
  );

  Serial.print(
    "MQTT Topic: "
  );

  Serial.println(
    topic
  );

  Serial.println();

  Serial.println(
    "JSON:"
  );

  Serial.println(
    json
  );

  Serial.println(
    "=========================================="
  );
}

// =====================================================
// PUBLISH MQTT
// =====================================================

bool publishJSON(
  const String &json,
  const char* topic
)
{
  if (
    !mqtt.connected()
  )
  {
    Serial.println(
      "[MQTT] Belum terhubung"
    );

    return false;
  }

  bool result =
    mqtt.publish(
      topic,
      json.c_str(),
      true
    );

  if (result)
  {
    Serial.println();

    Serial.println(
      "[MQTT] PUBLISH BERHASIL"
    );

    Serial.print(
      "[MQTT] Topic : "
    );

    Serial.println(
      topic
    );

    Serial.print(
      "[MQTT] Size  : "
    );

    Serial.print(
      json.length()
    );

    Serial.println(
      " byte"
    );

    return true;
  }

  Serial.println();

  Serial.println(
    "[MQTT] PUBLISH GAGAL"
  );

  Serial.print(
    "[MQTT] State : "
  );

  Serial.println(
    mqtt.state()
  );

  return false;
}

// =====================================================
// PROSES JSON DARI GATEWAY 1
// =====================================================

void processUARTData(
  String json
)
{
  json.trim();

  if (
    json.length() == 0
  )
  {
    return;
  }

  if (
    json.length() >= UART_BUFFER_SIZE
  )
  {
    Serial.println(
      "[UART] JSON terlalu besar"
    );

    return;
  }

  if (
    !isValidJSON(json)
  )
  {
    Serial.println();

    Serial.println(
      "[UART] Data bukan JSON"
    );

    Serial.print(
      "[UART] Data : "
    );

    Serial.println(
      json
    );

    return;
  }

  const char* topic =
    getMQTTTopic(
      json
    );

  if (
    topic == nullptr
  )
  {
    Serial.println();

    Serial.println(
      "[UART] Device tidak dikenal"
    );

    Serial.println(
      json
    );

    return;
  }

  printJSONInfo(
    json,
    topic
  );

  if (
    mqtt.connected()
  )
  {
    if (
      publishJSON(
        json,
        topic
      )
    )
    {
      Serial.println(
        "[UART] Data diteruskan ke MQTT"
      );
    }
    else
    {
      pendingJSON = json;
      pendingTopic = topic;
      hasPendingJSON = true;
    }
  }
  else
  {
    Serial.println(
      "[UART] MQTT belum terhubung"
    );

    Serial.println(
      "[UART] Data disimpan sementara"
    );

    pendingJSON = json;
    pendingTopic = topic;
    hasPendingJSON = true;
  }
}

// =====================================================
// BACA UART
// =====================================================

void readUART()
{
  while (
    GatewayUART.available()
  )
  {
    char c =
      GatewayUART.read();

    if (
      c == '\n'
    )
    {
      if (
        serialBuffer.length() > 0
      )
      {
        processUARTData(
          serialBuffer
        );
      }

      serialBuffer = "";
    }

    else if (
      c != '\r'
    )
    {
      if (
        serialBuffer.length()
        < UART_BUFFER_SIZE - 1
      )
      {
        serialBuffer += c;
      }
      else
      {
        Serial.println();

        Serial.println(
          "[UART] BUFFER OVERFLOW"
        );

        Serial.println(
          "[UART] JSON dibuang"
        );

        serialBuffer = "";
      }
    }
  }
}

// =====================================================
// RESET OTOMATIS 7 HARI
// =====================================================

void checkWeeklyReset()
{
  if (millis() - bootTime >= RESET_INTERVAL)
  {
    Serial.println();
    Serial.println("==========================================");
    Serial.println("RESET OTOMATIS SETELAH 7 HARI");
    Serial.println("Gateway 2 akan restart...");
    Serial.println("==========================================");

    delay(1000);

    ESP.restart();
  }
}

// =====================================================
// SETUP
// =====================================================

void setup()
{
  Serial.begin(
    115200
  );

  delay(1000);

  // ===================================================
  // MULAI HITUNG WAKTU UPTIME
  // ===================================================

  bootTime = millis();

  Serial.println();

  Serial.println(
    "=========================================="
  );

  Serial.println(
    "             ESP32 GATEWAY 2"
  );

  Serial.println(
    "              ESP32-ETH01"
  );

  Serial.println(
    "=========================================="
  );

  Serial.println(
    "          UART -> MQTT GATEWAY"
  );

  Serial.println(
    "=========================================="
  );

  // ===================================================
  // UART GATEWAY 1
  // ===================================================

  GatewayUART.begin(
    115200,
    SERIAL_8N1,
    RXD2,
    TXD2
  );

  Serial.println(
    "[UART] Gateway 1 interface aktif"
  );

  Serial.println(
    "[UART] RX = GPIO5"
  );

  Serial.println(
    "[UART] TX = GPIO17"
  );

  Serial.println(
    "[UART] Baudrate = 115200"
  );

  // ===================================================
  // NETWORK EVENT
  // ===================================================

  Network.begin();

  Network.onEvent(
    onNetworkEvent
  );

  // ===================================================
  // MQTT
  // ===================================================

  mqtt.setServer(
    MQTT_SERVER,
    MQTT_PORT
  );

  mqtt.setBufferSize(
    MQTT_BUFFER_SIZE
  );

  Serial.print(
    "[MQTT] Buffer = "
  );

  Serial.println(
    MQTT_BUFFER_SIZE
  );

  // ===================================================
  // WIFI
  // ===================================================

  startWiFi();

  // ===================================================
  // ETHERNET
  // ===================================================

  Serial.println();

  Serial.println(
    "[ETH] Starting Ethernet..."
  );

  bool ethResult =
    ETH.begin(
      ETH_PHY_LAN8720,
      1,
      23,
      18,
      16,
      ETH_CLOCK_GPIO0_IN
    );

  if (
    !ethResult
  )
  {
    Serial.println(
      "[ETH] ETH.begin GAGAL"
    );
  }
  else
  {
    Serial.println(
      "[ETH] ETH.begin berhasil"
    );
  }

  // ===================================================
  // INFO MQTT
  // ===================================================

  Serial.println();

  Serial.println(
    "=========================================="
  );

  Serial.print(
    "[MQTT] Server : "
  );

  Serial.println(
    MQTT_SERVER
  );

  Serial.print(
    "[MQTT] Port   : "
  );

  Serial.println(
    MQTT_PORT
  );

  Serial.print(
    "[MQTT] DIRIS  : "
  );

  Serial.println(
    MQTT_TOPIC_DIRIS
  );

  Serial.print(
    "[MQTT] ALERT  : "
  );

  Serial.println(
    MQTT_TOPIC_ALERT
  );

  Serial.println(
    "=========================================="
  );

  Serial.println();

  Serial.println(
    "Gateway 2 siap."
  );

  Serial.println(
    "Menunggu data dari Gateway 1..."
  );

  Serial.println();

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
  // BACA UART
  // ===================================================

  readUART();

  // ===================================================
  // CEK WIFI
  // ===================================================

  checkWiFi();

  // ===================================================
  // PILIH NETWORK
  // ===================================================

  selectNetwork();

  // ===================================================
  // MQTT CONNECT
  // ===================================================

  connectMQTT();

  // ===================================================
  // MQTT LOOP
  // ===================================================

  if (
    mqtt.connected()
  )
  {
    mqtt.loop();
  }

  delay(10);
}