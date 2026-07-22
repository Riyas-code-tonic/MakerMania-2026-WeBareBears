#ifndef A2DPSOURCEMANAGER_H
#define A2DPSOURCEMANAGER_H

#include <Arduino.h>
#include "DSPEngine.h"

extern "C"
{
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
}

class A2DPSourceManager
{
public:
    A2DPSourceManager();

    bool begin();
    bool connect();

    // Reconnect to previously saved MAC address (no discovery needed)
    bool reconnect();

    // Save the MAC address of the target device for reconnection
    void saveMac(const uint8_t* mac);

    // Check if A2DP stream is currently active
    bool isConnected();

    // Check if a reconnection attempt is needed
    bool needsReconnect();

private:

    static int32_t getAudioData(uint8_t *data, int32_t len);

    static void a2dpCallback(
        esp_a2d_cb_event_t event,
        esp_a2d_cb_param_t *param);

    static void avrcCallback(
        esp_avrc_ct_cb_event_t event,
        esp_avrc_ct_cb_param_t *param);

    // Connection state tracking
    static bool     _connected;
    static bool     _needsReconnect;
    static bool     _hasSavedMac;
    static uint8_t  _savedMac[6];
    static int      _reconnectRetries;
    static const int MAX_RECONNECT_RETRIES = 3;
};

#endif