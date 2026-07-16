// ═══════════════════════════════════════════════════════════════
//  Helmet Communication System — Main Sketch
//
//  Rider-to-Pillion one-way voice communication
//  INMP441 Mic → ESP32 DSP → Bluetooth A2DP → Earphones
//
//  Workflow:
//    1. Initialize Bluetooth stack (via BluetoothSerial)
//    2. GAP Discovery → auto-select closest audio device (RSSI)
//    3. A2DP connect → start media stream
//    4. Init I2S mic → calibrate noise floor
//    5. DSP loop: filter → VAD → suppress noise → stream
//    6. On disconnect: auto-reconnect to saved MAC
// ═══════════════════════════════════════════════════════════════

bool discoveryFinished = false;
#include "BluetoothManager.h"
#include "A2DPSourceManager.h"
#include "DSPEngine.h"

BluetoothManager btManager;
A2DPSourceManager a2dpManager;

// Track whether DSP has been initialized (only once)
bool dspInitialized = false;

// ═══════════════════════════════════════════════════════════
//  Discovery callback — auto-connect to closest audio device
// ═══════════════════════════════════════════════════════════
void onDiscoveryFinished()
{
    // Save MAC and connect to the strongest RSSI audio device
    BluetoothDevice dev = BluetoothManager::getTargetDevice();
    a2dpManager.saveMac(dev.mac);
    a2dpManager.connect();
}

// ═══════════════════════════════════════════════════════════
//  setup() — Initialize BT stack, A2DP, start discovery
// ═══════════════════════════════════════════════════════════
void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("========================================");
    Serial.println("  Helmet Communication System v1.0");
    Serial.println("  Rider → Pillion Voice Link");
    Serial.println("========================================");
    Serial.println();

    // Step 1: Initialize Bluetooth stack
    if (!btManager.begin())
    {
        Serial.println("FATAL: Bluetooth Manager init failed!");
        return;
    }
    BluetoothManager::setDiscoveryFinishedCallback(onDiscoveryFinished);

    // Step 2: Initialize A2DP source profile
    if (!a2dpManager.begin())
    {
        Serial.println("FATAL: A2DP Manager init failed!");
        return;
    }

    // Step 3: Start scanning for earphones
    delay(1000);
    Serial.println("Scanning for earphones...");
    Serial.println("Make sure earphones are in pairing mode!");
    Serial.println();
    btManager.startDiscovery(8);
}

// ═══════════════════════════════════════════════════════════
//  loop() — Main loop: handle reconnect + run DSP
// ═══════════════════════════════════════════════════════════
void loop()
{
    // ─── Handle reconnection if earphones disconnected ───
    if (a2dpManager.needsReconnect())
    {
        Serial.println("Waiting 2s before reconnect attempt...");
        delay(2000);

        if (!a2dpManager.reconnect())
        {
            // All retries exhausted — fall back to full discovery
            Serial.println("Reconnect failed — starting full re-scan...");
            btManager.startDiscovery(8);
        }
        return;   // Skip DSP this iteration while reconnecting
    }

    // ─── Handle discovery flag (legacy path) ─────────────
    if (discoveryFinished)
    {
        discoveryFinished = false;
        a2dpManager.connect();
        return;
    }

    // ─── Run DSP when connected ──────────────────────────
    if (a2dpManager.isConnected())
    {
        // Initialize I2S mic on first connection
        if (!dspInitialized)
        {
            dspSetup();
            dspInitialized = true;
            Serial.println("DSP engine started — speak to test!");
        }

        // Process one audio frame
        dspLoop();
    }
}