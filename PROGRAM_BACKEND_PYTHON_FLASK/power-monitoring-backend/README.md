# Power Monitoring Backend

Backend Python untuk sistem monitoring panel distribusi listrik 3 phasa dan alert pemadaman listrik. Backend ini subscribe ke topic MQTT dari ESP32 gateway dan menulis data ke InfluxDB.

## Arsitektur Sistem

```
┌─────────────────┐     ESP-NOW     ┌─────────────┐     MQTT      ┌─────────────────┐     InfluxDB     ┌─────────┐
│  ESP32 Panel    │ ──────────────► │  ESP32      │ ─────────────► │  Python Backend │ ──────────────► │ Grafana │
│  Monitor        │                 │  Gateway    │                │                 │                 │         │
└─────────────────┘                 └─────────────┘                └─────────────────┘                 └─────────┘
       │                                                                         ▲
       │ ESP-NOW                                                               │
       ▼                                                                         │
┌─────────────────┐     ESP-NOW                                                 │
│  ESP32 Outage   │ ──────────────────────────────────────────────────────────► │
│  Alert (x2)     │                                                             │
└─────────────────┘                                                             │
┌─────────────────┘                                                             │
│  ESP32 Outage   │ ──────────────────────────────────────────────────────────► │
│  Alert (x2)     │                                                             │
└─────────────────┘                                                             ▼
```

## Struktur Folder

```
power-monitoring-backend/
├── app/
│   ├── __init__.py
│   ├── config.py              # Konfigurasi dari environment variable
│   ├── mqtt/
│   │   ├── __init__.py
│   │   ├── client.py          # MQTT client dengan auto-reconnect
│   │   └── topics.py          # Pemetaan topic literal
│   ├── influx/
│   │   ├── __init__.py
│   │   ├── writer.py          # InfluxDB writer
│   │   └── schema.py          # Skema measurement InfluxDB
│   ├── models/
│   │   ├── __init__.py
│   │   ├── panel_data.py      # Dataclass untuk data panel 3 phasa
│   │   └── outage_data.py     # Dataclass untuk data outage
│   └── utils/
│       ├── __init__.py
│       ├── logger.py          # Konfigurasi logging
│       └── validators.py      # Validasi payload JSON
├── requirements.txt
├── run.py                     # Entry point utama
├── .env.example
└── README.md
```

## Topic MQTT

| Topic | QoS | Keterangan |
|-------|-----|------------|
| `panel/data` | 0 | Data panel 3 phasa (R, S, T) |
| `outage/gedungA/status` | 1 | Status pemadaman gedung A |
| `outage/gedungB/status` | 1 | Status pemadaman gedung B |
| `gateway/heartbeat` | 0 | Heartbeat dari gateway |

## Skema InfluxDB

### Measurement: `panel_3phase`
- **Tags**: `phase` (R/S/T)
- **Fields**: `voltage`, `current`, `power`, `power_factor`, `energy`

### Measurement: `outage_status`
- **Tags**: `gedung` (A/B)
- **Fields**: `status` (1=on, 0=off), `battery` (float 0-100)

## Persiapan

### 1. Buat Virtual Environment

```bash
# Buat virtual environment
python -m venv venv

# Aktifkan virtual environment
# Windows:
venv\Scripts\activate
# Linux/macOS:
source venv/bin/activate
```

### 2. Install Dependencies

```bash
pip install -r requirements.txt
```

### 3. Konfigurasi Environment Variables

Salin file `.env.example` ke `.env` dan isi dengan nilai yang sesuai:

```bash
cp .env.example .env
```

Edit file `.env`:

```env
# MQTT Broker
MQTT_HOST=192.168.1.100
MQTT_PORT=1883
MQTT_USERNAME=           # Kosongkan jika tidak ada auth
MQTT_PASSWORD=

# InfluxDB (Local)
INFLUXDB_URL=http://localhost:8086
INFLUXDB_TOKEN=your_token_here
INFLUXDB_ORG=myorg
INFLUXDB_BUCKET=power_monitoring
```

### 4. Konfigurasi InfluxDB Local

Pastikan InfluxDB sudah terinstall dan berjalan di komputer Anda.

**Generate API Token:**

```bash
# Login ke InfluxDB CLI (jika diminta setup)
influx auth create \
  --org myorg \
  --bucket power_monitoring \
  --description "Power Monitoring Backend" \
  --write-buckets
```

**Atau buat via UI:**
1. Buka `http://localhost:8086` (atau IP server jika remote)
2. Login dengan user admin
3. Buka **Load Data > Tokens**
4. Buat token baru dengan permission write ke bucket `power_monitoring`

### 5. Konfigurasi MQTT Broker

Pastikan MQTT broker (misalnya Mosquitto) berjalan dan dapat diakses dari machine yang menjalankan backend.

## Menjalankan Backend

```bash
# Pastikan virtual environment aktif
# Windows:
venv\Scripts\activate
# Linux/macOS:
source venv/bin/activate

# Jalankan backend
python run.py
```

## Logging

Backend menggunakan Python standard `logging` module. Output logging akan muncul di console.

Untuk logging ke file, set environment variable atau modifikasi `run.py`:

```python
setup_logging(log_to_file=True, log_file="power_monitoring.log")
```

## Troubleshooting

### Koneksi MQTT Gagal
- Pastikan MQTT broker sedang berjalan
- Periksa `MQTT_HOST` dan `MQTT_PORT` di `.env`
- Pastikan firewall mengizinkan koneksi ke port MQTT

### Koneksi InfluxDB Gagal
- Pastikan InfluxDB sedang berjalan: `influx ping`
- Periksa `INFLUXDB_URL` benar (http://localhost:8086 untuk lokal)
- Periksa token valid: `influx auth list`
- Pastikan organization dan bucket ada: `influx bucket list`

### Data Tidak Muncul di InfluxDB
- Periksa logging output untuk error
- Verifikasi format payload JSON dari ESP32 sesuai spesifikasi
- Pastikan timestamp yang dikirim device dalam format Unix epoch detik

## Lisensi

MIT License
