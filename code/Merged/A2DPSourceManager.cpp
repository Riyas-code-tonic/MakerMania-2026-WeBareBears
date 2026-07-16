#include <math.h>
#include "A2DPSourceManager.h"
#include "BluetoothManager.h"

// ═══════════════════════════════════════════════════════════
//  Static member initialization
// ═══════════════════════════════════════════════════════════
bool     A2DPSourceManager::_connected         = false;
bool     A2DPSourceManager::_needsReconnect    = false;
bool     A2DPSourceManager::_hasSavedMac       = false;
uint8_t  A2DPSourceManager::_savedMac[6]       = {0};
int      A2DPSourceManager::_reconnectRetries  = 0;

A2DPSourceManager::A2DPSourceManager()
{
}

// ═══════════════════════════════════════════════════════════
//  begin() — Initialize A2DP source + AVRCP profiles
// ═══════════════════════════════════════════════════════════
bool A2DPSourceManager::begin()
{
    Serial.println();
    esp_err_t ret;
    Serial.println("=================================");
    Serial.println("******** A2DP BEGIN CALLED ********");
    
    ret = esp_a2d_source_init();

    if (ret != ESP_OK)
    {
        Serial.printf("A2DP Source Initialization Failed : %d\n", ret);
        return false;
    }

    Serial.println("A2DP Source Initialized");
    ret = esp_avrc_ct_init();

    if (ret != ESP_OK)
    {
        Serial.printf("AVRCP Initialization Failed : %d\n", ret);
        return false;
    }

    Serial.println("AVRCP Initialized");
    ret = esp_avrc_ct_register_callback(avrcCallback);

    if (ret != ESP_OK)
    {
        Serial.printf("AVRCP Callback Registration Failed : %d\n", ret);
        return false;
    }

    Serial.println("AVRCP Callback Registered");

    Serial.println("=================================");

    ret = esp_a2d_register_callback(a2dpCallback);

    if (ret != ESP_OK)
    {
        Serial.printf("A2DP Callback Registration Failed : %d\n", ret);
        return false;
    }

    Serial.println("A2DP Callback Registered");

    ret = esp_a2d_source_register_data_callback(getAudioData);

    if (ret != ESP_OK)
    {
        Serial.printf("Audio Data Callback Registration Failed: %d\n", ret);
        return false;
    }

    Serial.println("Audio Data Callback Registered");

    return true;
}

// ═══════════════════════════════════════════════════════════
//  connect() — Connect to target device from discovery
// ═══════════════════════════════════════════════════════════
bool A2DPSourceManager::connect()
{
    BluetoothDevice device = BluetoothManager::getTargetDevice();

    Serial.println();
    Serial.println("========== Connecting ==========");

    Serial.print("Device : ");
    Serial.println(device.name);

    Serial.print("RSSI : ");
    Serial.println(device.rssi);

    // Save MAC for future reconnection
    saveMac(device.mac);

    esp_err_t ret = esp_a2d_source_connect(device.mac);

    if (ret != ESP_OK)
    {
        Serial.printf("Connection request failed : %d\n", ret);
        return false;
    }

    Serial.println("Connection request sent");
    return true;
}

// ═══════════════════════════════════════════════════════════
//  reconnect() — Reconnect to saved MAC address directly
// ═══════════════════════════════════════════════════════════
bool A2DPSourceManager::reconnect()
{
    if (!_hasSavedMac)
    {
        Serial.println("No saved MAC address — cannot reconnect");
        return false;
    }

    _reconnectRetries++;

    if (_reconnectRetries > MAX_RECONNECT_RETRIES)
    {
        Serial.println("Max reconnect retries exceeded — need re-scan");
        _reconnectRetries = 0;
        _needsReconnect = false;
        return false;   // Caller should trigger discovery
    }

    Serial.printf("Reconnecting to saved MAC (attempt %d/%d)...\n",
                  _reconnectRetries, MAX_RECONNECT_RETRIES);

    Serial.print("MAC: ");
    for (int i = 0; i < 6; i++)
    {
        Serial.printf("%02X", _savedMac[i]);
        if (i != 5) Serial.print(":");
    }
    Serial.println();

    esp_err_t ret = esp_a2d_source_connect(_savedMac);

    if (ret != ESP_OK)
    {
        Serial.printf("Reconnection request failed : %d\n", ret);
        return false;
    }

    Serial.println("Reconnection request sent");
    _needsReconnect = false;
    return true;
}

// ═══════════════════════════════════════════════════════════
//  saveMac() — Store MAC for reconnection
// ═══════════════════════════════════════════════════════════
void A2DPSourceManager::saveMac(const uint8_t* mac)
{
    memcpy(_savedMac, mac, 6);
    _hasSavedMac = true;

    Serial.print("MAC saved for reconnection: ");
    for (int i = 0; i < 6; i++)
    {
        Serial.printf("%02X", _savedMac[i]);
        if (i != 5) Serial.print(":");
    }
    Serial.println();
}

bool A2DPSourceManager::isConnected()
{
    return _connected;
}

bool A2DPSourceManager::needsReconnect()
{
    return _needsReconnect;
}

// ═══════════════════════════════════════════════════════════
//  A2DP Callback — Connection and audio state events
// ═══════════════════════════════════════════════════════════
void A2DPSourceManager::a2dpCallback(
    esp_a2d_cb_event_t event,
    esp_a2d_cb_param_t *param)
{
    Serial.print("A2DP Event : ");
    Serial.println(event);

    switch (event)
    {
        case ESP_A2D_CONNECTION_STATE_EVT:
        {
            Serial.print("Connection State = ");
            Serial.println(param->conn_stat.state);

            Serial.print("Disconnect Reason = ");
            Serial.println(param->conn_stat.disc_rsn);

            if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED)
            {
                _connected = true;
                _reconnectRetries = 0;
                _needsReconnect = false;
                Serial.println("=== A2DP CONNECTED — Starting Media Stream ===");
                esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
            }
            else if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED)
            {
                _connected = false;
                Serial.println("=== A2DP DISCONNECTED ===");

                // Set flag for main loop to handle reconnection
                // (ESP APIs should not be called from callback context)
                if (_hasSavedMac)
                {
                    Serial.println("Will attempt reconnect to saved MAC...");
                    _needsReconnect = true;
                }
            }

            break;
        }

        case ESP_A2D_AUDIO_STATE_EVT:
        {
            Serial.print("Audio State = ");
            Serial.println(param->audio_stat.state);
            break;
        }

        case ESP_A2D_PROF_STATE_EVT:
            Serial.println("Profile State Changed");
            break;
            
        case ESP_A2D_MEDIA_CTRL_ACK_EVT:
        {
            Serial.print("Media Control ACK = ");
            Serial.println(param->media_ctrl_stat.cmd);
            break;
        }

        default:
            break;
    }
}

// ═══════════════════════════════════════════════════════════
//  getAudioData() — Called by BT stack to get SBC audio data
//  Drains the ring buffer filled by DSPEngine
// ═══════════════════════════════════════════════════════════
int32_t A2DPSourceManager::getAudioData(uint8_t *data, int32_t len)
{
    static int count = 0;

    if (++count % 500 == 0)
    {
        Serial.println("Streaming audio...");
    }

    int16_t *samples = (int16_t *)data;
    int sampleCount = len / 2;

    for (int i = 0; i < sampleCount; i += 2)
    {
        int16_t sample = 0;

        // Read from ring buffer if data available
        if (ringHead != ringTail)
        {
            sample   = ringBuf[ringTail];
            ringTail = (ringTail + 1) % RING_SIZE;
        }

        samples[i]     = sample;   // Left channel
        samples[i + 1] = sample;   // Right channel (mono duplicated)
    }

    return len;
}

// ═══════════════════════════════════════════════════════════
//  AVRCP Callback — placeholder for future use
// ═══════════════════════════════════════════════════════════
void A2DPSourceManager::avrcCallback(
    esp_avrc_ct_cb_event_t event,
    esp_avrc_ct_cb_param_t *param)
{
    // Placeholder for future AVRCP events
}