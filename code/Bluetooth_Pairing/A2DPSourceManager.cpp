#include <math.h>
#include "A2DPSourceManager.h"
#include "BluetoothManager.h"

A2DPSourceManager::A2DPSourceManager()
{
}

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

ret = esp_a2d_source_register_data_callback(getAudioData);

if (ret != ESP_OK)
{
    Serial.printf("Audio Data Callback Registration Failed: %d\n", ret);
    return false;
}

Serial.println("Audio Data Callback Registered");

if (ret != ESP_OK)
{
    Serial.printf("A2DP Callback Registration Failed : %d\n", ret);
    return false;
}

Serial.println("A2DP Callback Registered");

    return true;
}
bool A2DPSourceManager::connect()
{
    BluetoothDevice device = BluetoothManager::getTargetDevice();

    Serial.println();
    Serial.println("========== Connecting ==========");

    Serial.print("Device : ");
    Serial.println(device.name);

    Serial.print("RSSI : ");
    Serial.println(device.rssi);
    esp_err_t ret = esp_a2d_source_connect(device.mac);

if (ret != ESP_OK)
{
    Serial.printf("Connection request failed : %d\n", ret);
    return false;
}

Serial.println("Connection request sent");

return true;

}

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
        Serial.println("Starting Media Stream...");
        esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
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

static float phase = 0.0f;

const float SAMPLE_RATE = 44100.0f;
const float TONE_FREQ   = 1000.0f;
const float AMPLITUDE   = 12000.0f;

int32_t A2DPSourceManager::getAudioData(uint8_t *data, int32_t len)
{
    static int count = 0;

if (++count % 500 == 0)
{
    Serial.println("Streaming...");

}

static bool printed = false;

if (!printed)
{
    Serial.print("Requested bytes = ");
    Serial.println(len);
    printed = true;
}

    int16_t *samples = (int16_t *)data;

    int sampleCount = len / 2;

    for (int i = 0; i < sampleCount; i += 2)
    {
        int16_t sample = (int16_t)(AMPLITUDE * sinf(phase));

        samples[i] = sample;       // Left channel
        samples[i + 1] = sample;   // Right channel

        phase += 2.0f * M_PI * TONE_FREQ / SAMPLE_RATE;

        if (phase >= 2.0f * M_PI)
            phase -= 2.0f * M_PI;
    }

    return len;
}
void A2DPSourceManager::avrcCallback(
    esp_avrc_ct_cb_event_t event,
    esp_avrc_ct_cb_param_t *param)
{
    // Placeholder for future AVRCP events
}