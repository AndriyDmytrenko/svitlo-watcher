#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <secrets.h>

// ============================================================================
// CONFIGURATION
// ============================================================================
// WiFi credentials, device hostname, and API endpoints are defined in secrets.h

// LED configuration
const int BLUE_LED_PIN = 2;   // Blue LED (success indicator)
const int RED_LED_PIN = 13;   // Red LED (error indicator) - common on ESP32 dev boards

// API endpoints to poll (defined in secrets.h)
const char* API_ENDPOINTS[] = {
    API_ENDPOINT_1,
    API_ENDPOINT_2,
};
const int NUM_ENDPOINTS = sizeof(API_ENDPOINTS) / sizeof(API_ENDPOINTS[0]);

// Timing configuration
const unsigned long POLL_INTERVAL_MS = 30000;  // Poll every 30 seconds
const int HTTP_TIMEOUT_MS = 5000;              // 5 second timeout for HTTP requests
const int WIFI_RECONNECT_DELAY_MS = 5000;      // Wait 5 seconds before WiFi reconnect

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

unsigned long lastPollTime = 0;
SemaphoreHandle_t ledMutex;  // Mutex for thread-safe LED control
int activeRequests = 0;       // Counter for active HTTP requests
int failedRequests = 0;       // Counter for failed requests

// ============================================================================
// TASK PARAMETER STRUCTURE
// ============================================================================

struct RequestTaskParams {
    const char* url;
    int index;
};

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

void connectToWiFi();
void checkWiFiConnection();
void pollEndpoints();
void sendGetRequestTask(void* parameter);
void sendGetRequest(const char* url, int index);
void blinkBlueLED(int times, int delayMs);

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Initialize LEDs (standard logic: HIGH=ON, LOW=OFF)
    pinMode(BLUE_LED_PIN, OUTPUT);
    pinMode(RED_LED_PIN, OUTPUT);
    digitalWrite(BLUE_LED_PIN, LOW);   // Turn off blue LED
    digitalWrite(RED_LED_PIN, LOW);    // Turn off red LED
    
    // Create mutex for thread-safe LED control
    ledMutex = xSemaphoreCreateMutex();
    
    Serial.println("\n\n========================================");
    Serial.println("ESP32 WiFi API Poller");
    Serial.println("========================================");
    
    // Configure WiFi - explicitly disable AP and ensure Station mode only
    WiFi.disconnect(true);  // Disconnect and clear saved WiFi config
    WiFi.mode(WIFI_OFF);    // Turn off WiFi completely first
    delay(100);
    WiFi.mode(WIFI_STA);    // Set to Station mode only (no AP)
    WiFi.setAutoReconnect(true);
    
    Serial.println("WiFi configured: Station mode only (AP disabled)");
    
    // Set device hostname for network identification (must be before WiFi.begin)
    WiFi.setHostname(DEVICE_HOSTNAME);
    Serial.print("Device hostname set to: ");
    Serial.println(DEVICE_HOSTNAME);
    
    Serial.println("SSL/TLS: Using insecure mode (certificate validation disabled)");
    Serial.println("Each HTTP task will create its own secure client");
    
    // Initial WiFi connection
    connectToWiFi();
    
    // Poll endpoints immediately after boot
    if (WiFi.status() == WL_CONNECTED) {
        pollEndpoints();
        lastPollTime = millis();  // Reset timer for next poll
    }
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    // Check WiFi connection status
    checkWiFiConnection();
    
    // Check if it's time to poll endpoints
    unsigned long currentTime = millis();
    if (currentTime - lastPollTime >= POLL_INTERVAL_MS) {
        lastPollTime = currentTime;
        pollEndpoints();
    }
    
    delay(100);  // Small delay to prevent watchdog issues
}

// ============================================================================
// WIFI FUNCTIONS
// ============================================================================

void connectToWiFi() {
    Serial.print("Connecting to WiFi: ");
    Serial.println(WIFI_SSID);
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    const int MAX_ATTEMPTS = 30;  // 15 seconds total
    
    while (WiFi.status() != WL_CONNECTED && attempts < MAX_ATTEMPTS) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✓ WiFi connected successfully!");
        Serial.print("Hostname: ");
        Serial.println(WiFi.getHostname());
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        Serial.print("MAC Address: ");
        Serial.println(WiFi.macAddress());
        Serial.print("Signal Strength (RSSI): ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
        
        // Turn off error LED and blink blue LED to indicate successful connection
        digitalWrite(RED_LED_PIN, LOW);   // Turn off red LED
        blinkBlueLED(3, 200);             // Blink blue LED 3 times
    } else {
        Serial.println("\n✗ WiFi connection failed!");
        Serial.println("Will retry in next cycle...");
        
        // Turn on red LED to indicate WiFi error
        digitalWrite(RED_LED_PIN, HIGH);
    }
}

void checkWiFiConnection() {
    static unsigned long lastCheckTime = 0;
    static bool wasConnected = false;
    unsigned long currentTime = millis();
    
    // Check WiFi status every second
    if (currentTime - lastCheckTime >= 1000) {
        lastCheckTime = currentTime;
        
        if (WiFi.status() != WL_CONNECTED) {
            if (wasConnected) {
                Serial.println("\n⚠ WiFi connection lost! Attempting to reconnect...");
                wasConnected = false;
                
                // Turn on red LED to indicate WiFi error
                digitalWrite(RED_LED_PIN, HIGH);
            }
            
            connectToWiFi();
        } else {
            if (!wasConnected) {
                wasConnected = true;
                Serial.println("WiFi reconnected successfully!");
                
                // Turn off red LED on successful reconnection
                digitalWrite(RED_LED_PIN, LOW);
            }
        }
    }
}

// ============================================================================
// API POLLING FUNCTIONS
// ============================================================================

void pollEndpoints() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("⚠ Cannot poll endpoints - WiFi not connected");
        
        // Turn on red LED to indicate error
        if (xSemaphoreTake(ledMutex, portMAX_DELAY)) {
            digitalWrite(RED_LED_PIN, HIGH);
            xSemaphoreGive(ledMutex);
        }
        return;
    }
    
    Serial.println("\n========================================");
    Serial.println("Starting PARALLEL API poll cycle");
    Serial.println("========================================");
    
    // Reset counters
    activeRequests = NUM_ENDPOINTS;
    failedRequests = 0;
    
    // Create tasks for parallel HTTP requests
    for (int i = 0; i < NUM_ENDPOINTS; i++) {
        RequestTaskParams* params = new RequestTaskParams();
        params->url = API_ENDPOINTS[i];
        params->index = i + 1;
        
        // Create a FreeRTOS task for each endpoint
        char taskName[32];
        snprintf(taskName, sizeof(taskName), "HTTPTask_%d", i + 1);
        
        xTaskCreate(
            sendGetRequestTask,   // Task function
            taskName,             // Task name
            8192,                 // Stack size (bytes)
            (void*)params,        // Task parameters
            1,                    // Priority
            NULL                  // Task handle (not needed)
        );
        
        Serial.print("[");
        Serial.print(i + 1);
        Serial.print("/");
        Serial.print(NUM_ENDPOINTS);
        Serial.print("] Launched task for: ");
        Serial.println(API_ENDPOINTS[i]);
    }
    
    // Wait for all tasks to complete
    while (activeRequests > 0) {
        delay(50);
    }
    
    Serial.println("\n========================================");
    Serial.print("Poll cycle complete - ");
    if (failedRequests > 0) {
        Serial.print(failedRequests);
        Serial.println(" request(s) failed");
    } else {
        Serial.println("All requests successful");
    }
    Serial.println("========================================\n");
}

// Task wrapper for FreeRTOS
void sendGetRequestTask(void* parameter) {
    RequestTaskParams* params = (RequestTaskParams*)parameter;
    sendGetRequest(params->url, params->index);
    delete params;
    
    // Decrement active request counter
    activeRequests--;
    
    // Delete this task
    vTaskDelete(NULL);
}

void sendGetRequest(const char* url, int index) {
    // Create a dedicated WiFiClientSecure for this task
    WiFiClientSecure* wifiClient = new WiFiClientSecure();
    wifiClient->setInsecure();
    
    HTTPClient http;
    
    // Configure HTTP client
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.setConnectTimeout(HTTP_TIMEOUT_MS);
    
    // Begin HTTP request
    if (!http.begin(*wifiClient, url)) {
        Serial.print("[");
        Serial.print(index);
        Serial.println("] ✗ Failed to initialize HTTP client");
        
        // Turn on red LED to indicate error
        if (xSemaphoreTake(ledMutex, portMAX_DELAY)) {
            digitalWrite(RED_LED_PIN, HIGH);
            xSemaphoreGive(ledMutex);
        }
        failedRequests++;
        
        http.end();
        delete wifiClient;
        return;
    }
    
    // Set custom User-Agent (must use setUserAgent, not addHeader)
    String userAgent = String(DEVICE_HOSTNAME) + "/1.0";
    http.setUserAgent(userAgent.c_str());
    http.addHeader("Accept", "application/json");
    
    // Send GET request
    Serial.print("[");
    Serial.print(index);
    Serial.print("] Sending GET request... ");
    int httpCode = http.GET();
    
    // Handle response
    if (httpCode > 0) {
        Serial.print("[");
        Serial.print(index);
        Serial.print("] Response code: ");
        Serial.println(httpCode);
        
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            Serial.print("[");
            Serial.print(index);
            Serial.print("] ✓ Success! Response length: ");
            Serial.print(payload.length());
            Serial.println(" bytes");
            
            // Turn off red LED on successful request (if all requests succeed)
            if (xSemaphoreTake(ledMutex, portMAX_DELAY)) {
                if (failedRequests == 0) {
                    digitalWrite(RED_LED_PIN, LOW);
                }
                xSemaphoreGive(ledMutex);
            }
        } else {
            Serial.print("[");
            Serial.print(index);
            Serial.print("] ⚠ HTTP error code: ");
            Serial.println(httpCode);
            
            // Turn on red LED for HTTP errors
            if (xSemaphoreTake(ledMutex, portMAX_DELAY)) {
                digitalWrite(RED_LED_PIN, HIGH);
                xSemaphoreGive(ledMutex);
            }
            failedRequests++;
        }
    } else {
        Serial.print("[");
        Serial.print(index);
        Serial.print("] ✗ Request failed: ");
        Serial.println(http.errorToString(httpCode).c_str());
        
        // Turn on red LED for request failures
        if (xSemaphoreTake(ledMutex, portMAX_DELAY)) {
            digitalWrite(RED_LED_PIN, HIGH);
            xSemaphoreGive(ledMutex);
        }
        failedRequests++;
        
        // Common error codes
        if (httpCode == HTTPC_ERROR_CONNECTION_REFUSED) {
            Serial.print("[");
            Serial.print(index);
            Serial.println("]   → Connection refused by server");
        } else if (httpCode == HTTPC_ERROR_CONNECTION_LOST) {
            Serial.print("[");
            Serial.print(index);
            Serial.println("]   → Connection lost during request");
        } else if (httpCode == HTTPC_ERROR_READ_TIMEOUT) {
            Serial.print("[");
            Serial.print(index);
            Serial.println("]   → Read timeout exceeded");
        }
    }
    
    // Clean up
    http.end();
    delete wifiClient;
}

// ============================================================================
// LED FUNCTIONS
// ============================================================================

void blinkBlueLED(int times, int delayMs) {
    for (int i = 0; i < times; i++) {
        digitalWrite(BLUE_LED_PIN, HIGH);  // Turn blue LED on
        delay(delayMs);
        digitalWrite(BLUE_LED_PIN, LOW);   // Turn blue LED off
        delay(delayMs);
    }
}
