#ifndef A2DPSOURCEMANAGER_H
#define A2DPSOURCEMANAGER_H

#include <Arduino.h>

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

private:

static int32_t getAudioData(uint8_t *data, int32_t len);

    static void a2dpCallback(
        esp_a2d_cb_event_t event,
        esp_a2d_cb_param_t *param);

    static void avrcCallback(
        esp_avrc_ct_cb_event_t event,
        esp_avrc_ct_cb_param_t *param);
};

#endif