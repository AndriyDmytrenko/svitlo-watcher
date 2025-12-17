# ESP32 Svitlo Watcher

A parallel HTTP health check monitoring system for ESP32 that continuously polls multiple API endpoints and provides visual feedback through LED indicators.

## 🎯 Project Goals

This project was created to monitor the availability of critical services by sending periodic health check pings to multiple endpoints. The ESP32 device:

- Connects to WiFi and maintains a stable connection with auto-reconnect
- Polls multiple HTTPS endpoints in parallel using FreeRTOS tasks
- Provides immediate visual feedback through LED indicators
- Operates autonomously without requiring external infrastructure
- Can be deployed in remote locations for continuous monitoring

## ✨ Features

- **Parallel Polling**: Utilizes FreeRTOS tasks to send HTTP requests simultaneously to multiple endpoints
- **Thread-Safe LED Control**: Mutex-protected LED operations for safe access from multiple tasks
- **Visual Status Indicators**:
  - Blue LED: Blinks 3 times on successful WiFi connection
  - Red LED: Continuously lit during errors, turns off when resolved
- **HTTPS Support**: Uses `WiFiClientSecure` with configurable SSL/TLS settings
- **Auto-Reconnect**: Automatically recovers from WiFi disconnections
- **Configurable Hostname**: Device identifies itself on the network with a custom hostname
- **Custom User-Agent**: HTTP requests include a custom User-Agent header for identification
- **Fast Polling**: 30-second intervals with immediate polling on boot
- **Secure Configuration**: WiFi credentials and API endpoints stored separately from code

## 🛠 Hardware Requirements

- **ESP32 Development Board** (ESP32-D0WD-V3 or compatible)
- **Blue LED** connected to GPIO 2 (or use built-in LED)
- **Red LED** connected to GPIO 13
- **USB-to-Serial adapter** for programming (typically built into dev boards)

### Recommended Boards

- ESP32 DevKit V1
- ESP32-WROOM-32
- NodeMCU-32S
- Any ESP32 board with at least 4MB Flash

## 💻 Software & Tools

### Development Tools

- **[PlatformIO](https://platformio.org/)**: Cross-platform build system and IDE
  - Version used: Core 6.12.0
  - Can be used with VS Code, CLion, or standalone
- **[Visual Studio Code](https://code.visualstudio.com/)** (recommended): With PlatformIO IDE extension

### Platform & Framework

- **Platform**: Espressif 32 (espressif32) v6.12.0
- **Framework**: Arduino for ESP32 v3.20017.241212
- **Board**: ESP32 Dev Module (esp32dev)
- **Toolchain**: xtensa-esp32 GCC 8.4.0

### Built-in Libraries Used

All libraries are included with the ESP32 Arduino framework:
- **WiFi** v2.0.0 - WiFi connectivity
- **HTTPClient** v2.0.0 - HTTP/HTTPS requests
- **WiFiClientSecure** v2.0.0 - Secure HTTPS connections

### Upload Tool

- **esptool.py** v4.9.0 - ESP32 firmware flashing utility

## 🚀 Getting Started

### 1. Clone the Repository

```bash
git clone <repository-url>
cd svitlo-watcher
```

### 2. Configure Secrets

Copy the example secrets file and configure your settings:

```bash
cp include/secrets.h.example include/secrets.h
```

Edit `include/secrets.h` with your configuration:

```cpp
// WiFi credentials
#define WIFI_SSID "your-wifi-ssid"
#define WIFI_PASSWORD "your-wifi-password"

// Device identification
#define DEVICE_HOSTNAME "ESP32-Svitlo-Watcher"

// API endpoints to poll
#define API_ENDPOINT_1 "https://hc-ping.com/your-first-endpoint-uuid"
#define API_ENDPOINT_2 "https://hc-ping.com/your-second-endpoint-uuid"
```

> **Note**: The `secrets.h` file is excluded from version control via `.gitignore` to protect your credentials.

### 3. Install PlatformIO

If you haven't already:

**Using VS Code:**
1. Install [Visual Studio Code](https://code.visualstudio.com/)
2. Install the "PlatformIO IDE" extension
3. Restart VS Code

**Using CLI:**
```bash
pip install platformio
```

### 4. Build the Project

```bash
platformio run --environment esp32dev
```

Or use the PlatformIO IDE build button in VS Code.

### 5. Upload to ESP32

Connect your ESP32 board via USB and run:

```bash
platformio run --target upload --environment esp32dev
```

If you have multiple devices, specify the port:

```bash
platformio run --target upload --environment esp32dev --upload-port /dev/cu.usbserial-10
```

> **Note**: On Windows, ports are typically `COM1`, `COM3`, etc. On macOS/Linux, they're usually `/dev/ttyUSB0`, `/dev/cu.usbserial-*`, etc.

### 6. Monitor Serial Output

```bash
platformio device monitor --environment esp32dev --port /dev/cu.usbserial-10
```

Or use the PlatformIO IDE monitor button in VS Code.

## 📊 Configuration Options

All configuration is in `src/main.cpp`:

```cpp
// LED configuration
const int BLUE_LED_PIN = 2;   // Blue LED (success indicator)
const int RED_LED_PIN = 13;   // Red LED (error indicator)

// Timing configuration
const unsigned long POLL_INTERVAL_MS = 30000;  // Poll every 30 seconds
const int HTTP_TIMEOUT_MS = 5000;              // 5 second timeout
const int WIFI_RECONNECT_DELAY_MS = 5000;      // Wait 5 seconds before reconnect
```

## 🚦 LED Indicators

### Blue LED (GPIO 2)
- **Blinks 3 times**: WiFi successfully connected
- **Off**: Normal operation (WiFi connected, polling endpoints)

### Red LED (GPIO 13)
- **On**: Error condition (WiFi disconnected, HTTP request failed, or connection issue)
- **Off**: All systems operational

## 🏗 Architecture

### Parallel Polling Design

The system uses **FreeRTOS tasks** to achieve true parallel HTTP requests:

1. **Main Loop**: Checks WiFi status and triggers poll cycles every 30 seconds
2. **Poll Cycle**: Creates independent tasks for each endpoint
3. **HTTP Tasks**: Each task has its own `WiFiClientSecure` instance for concurrent HTTPS connections
4. **Thread Safety**: LED operations are protected by a mutex (`SemaphoreHandle_t`)

### Key Components

```
┌─────────────────────────────────────────┐
│          Main Loop (Arduino)            │
│  - WiFi connection management           │
│  - Poll interval timing                 │
└──────────────────┬──────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────┐
│         pollEndpoints()                 │
│  - Creates FreeRTOS tasks               │
│  - Waits for all tasks to complete      │
└──────────────────┬──────────────────────┘
                   │
        ┌──────────┴──────────┬───────────┐
        ▼                     ▼           ▼
    [Task 1]              [Task 2]    [Task N]
    HTTP GET              HTTP GET    HTTP GET
    Endpoint 1            Endpoint 2  Endpoint N
        │                     │           │
        └──────────┬──────────┴───────────┘
                   ▼
           LED Status Update
          (Mutex Protected)
```

### Memory Usage

- **RAM**: ~46KB (14.3% of 320KB)
- **Flash**: ~919KB (70.1% of 1.3MB)
- **Stack per Task**: 8KB

## 🔧 Troubleshooting

### Build Errors

**Problem**: `secrets.h: No such file or directory`
- **Solution**: Copy `include/secrets.h.example` to `include/secrets.h` and configure it

**Problem**: Compilation errors with WiFi libraries
- **Solution**: Ensure you're using ESP32 board. WiFi libraries are built-in for ESP32.

### Upload Errors

**Problem**: `Failed to connect to ESP32`
- **Solution**: Hold the BOOT button on ESP32 while uploading
- Check USB cable (some cables are power-only)
- Try a different USB port
- Verify correct port in upload command

**Problem**: `Permission denied` on serial port (Linux/Mac)
- **Solution**: Add user to dialout group: `sudo usermod -a -G dialout $USER`
- Or use sudo for upload (not recommended)

### Runtime Issues

**Problem**: WiFi won't connect
- Check SSID and password in `secrets.h`
- Verify WiFi is 2.4GHz (ESP32 doesn't support 5GHz)
- Check WiFi signal strength

**Problem**: HTTPS requests fail
- Verify URLs are correct in `secrets.h`
- Check if endpoints are accessible from your network
- Increase `HTTP_TIMEOUT_MS` if network is slow

**Problem**: Red LED stays on
- Check serial monitor output for specific error messages
- Verify internet connectivity
- Test endpoints manually with curl/browser

## 📈 Performance

- **Parallel Execution**: All endpoints are polled simultaneously
- **Response Time**: Typically 200-500ms per request (depends on network and endpoint)
- **Poll Interval**: 30 seconds (configurable)
- **First Poll**: Immediate after boot (no initial delay)
- **WiFi Auto-Reconnect**: ~5-15 seconds to recover from disconnection

## 🔐 Security Considerations

- **Secrets Management**: WiFi credentials and API endpoints are stored in `secrets.h`, which is excluded from version control
- **HTTPS**: All requests use HTTPS with `WiFiClientSecure`
- **Certificate Validation**: Currently disabled (`setInsecure()`) for compatibility. For production, consider implementing proper certificate validation.
- **Network Security**: Device operates in Station mode only (AP mode explicitly disabled)

## 📝 Serial Monitor Output Example

```
========================================
ESP32 WiFi API Poller
========================================
WiFi configured: Station mode only (AP disabled)
Device hostname set to: ESP32-Svitlo-Watcher
SSL/TLS: Using insecure mode (certificate validation disabled)
Each HTTP task will create its own secure client
Connecting to WiFi: marvin
..........
✓ WiFi connected successfully!
Hostname: ESP32-Svitlo-Watcher
IP Address: 192.168.1.100
MAC Address: 1C:69:20:E9:14:F0
Signal Strength (RSSI): -45 dBm

========================================
Starting PARALLEL API poll cycle
========================================
[1/2] Launched task for: https://hc-ping.com/7b7bf66f-...
[2/2] Launched task for: https://hc-ping.com/5466dad8-...
[1] Sending GET request... [1] Response code: 200
[1] ✓ Success! Response length: 2 bytes
[2] Sending GET request... [2] Response code: 200
[2] ✓ Success! Response length: 2 bytes

========================================
Poll cycle complete - All requests successful
========================================
```

## 🤝 Contributing

Contributions are welcome! Areas for improvement:

- Add support for more than 2 endpoints (configurable array)
- Implement proper SSL certificate validation
- Add mDNS support for easier device discovery
- Web interface for configuration
- Support for POST requests with custom payloads
- Persistent configuration using ESP32 preferences
- OTA (Over-The-Air) firmware updates

## 📄 License

This project is open source. Feel free to use, modify, and distribute as needed.

## 🙏 Acknowledgments

- Built with [PlatformIO](https://platformio.org/)
- Uses [Arduino framework for ESP32](https://github.com/espressif/arduino-esp32)
- Health check monitoring via [healthchecks.io](https://healthchecks.io/)

---

**Project Status**: ✅ Production Ready

**Last Updated**: December 2024
