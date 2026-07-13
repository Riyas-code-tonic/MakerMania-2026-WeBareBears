#include "BluetoothManager.h"

std::vector<BluetoothDevice> BluetoothManager::devices;
BluetoothDevice BluetoothManager::targetDevice;
void (*BluetoothManager::discoveryFinishedCallback)() = nullptr;

BluetoothManager::BluetoothManager()
{
}

bool BluetoothManager::begin()
{
    Serial.println();
    Serial.println("=================================");
    Serial.println(" Bluetooth Manager Started");
    Serial.println("=================================");

    // Start Bluetooth Stack
    if (!serialBT.begin("Helmet_Communication"))
    {
        Serial.println("Bluetooth Stack Failed to Start");
        return false;
    }

    Serial.println("Bluetooth Stack Started");

    esp_bt_gap_set_scan_mode(
      ESP_BT_CONNECTABLE,
      ESP_BT_GENERAL_DISCOVERABLE);

    // Register GAP Callback
    esp_err_t ret = esp_bt_gap_register_callback(gapCallback);

    if (ret != ESP_OK)
    {
        Serial.printf("Failed to register GAP callback : %d\n", ret);
        return false;
    }

    Serial.println("GAP Callback Registered");

    return true;
}
void BluetoothManager::setDiscoveryFinishedCallback(void (*callback)())
{
    discoveryFinishedCallback = callback;
}

void BluetoothManager::startDiscovery(uint8_t duration)
{
    devices.clear();

    Serial.println();
    Serial.println("Starting     Bluetooth Discovery...");

    esp_err_t ret = esp_bt_gap_start_discovery(
                        ESP_BT_INQ_MODE_GENERAL_INQUIRY,
                        duration,
                        0);

if (ret != ESP_OK)
    {
        Serial.printf("Discovery Failed : %d\n", ret);
        return;
    }

    Serial.println("Discovery Started");
}

void BluetoothManager::stopDiscovery()
{
    esp_bt_gap_cancel_discovery();
}

void BluetoothManager::printDevices()
{
    Serial.println();
    

    if (devices.empty())
    {
        Serial.println("No devices found.");
        return;
    }
    bool firstDevice = true;

    for (BluetoothDevice &d : devices)
    {
        if (!isTargetAudioDevice(d.cod))
{
    continue;
}
if (firstDevice || d.rssi > targetDevice.rssi)
{
    targetDevice = d;
    firstDevice = false;
}
        Serial.println("------------------------");

        Serial.print("Name : ");
        Serial.println(d.name);

        Serial.print("MAC  : ");

        for (int i = 0; i < 6; i++)
        {
            Serial.printf("%02X", d.mac[i]);

            if (i != 5)
                Serial.print(":");
        }

        Serial.println();

        Serial.print("RSSI : ");
        Serial.println(d.rssi);

        Serial.print("Type : ");
           Serial.println(getDeviceType(d.cod));
    }
    Serial.println();
Serial.println("========== Selected Device ==========");

if (!firstDevice)
{
    Serial.print("Name : ");
    Serial.println(targetDevice.name);

    Serial.print("RSSI : ");
    Serial.println(targetDevice.rssi);
}
else
{
    Serial.println("No compatible audio device found.");
}
}

String BluetoothManager::getDeviceType(uint32_t cod)
{
    uint8_t majorClass = (cod >> 8) & 0x1F;

    switch (majorClass)
    {
        case 0x01: return "Computer";
        case 0x02: return "Phone";
        case 0x03: return "LAN/Network";
        case 0x04: return "Audio/Video";
        case 0x05: return "Peripheral";
        case 0x06: return "Imaging";
        case 0x07: return "Wearable";
        case 0x08: return "Toy";
        case 0x09: return "Health";
        default:   return "Unknown";
    }
}

bool BluetoothManager::isTargetAudioDevice(uint32_t cod)
{
    // Major Class (bits 8-12)
    uint8_t majorClass = (cod >> 8) & 0x1F;

    // Minor Class (bits 2-7)
    uint8_t minorClass = (cod >> 2) & 0x3F;

    // Must be Audio/Video
    if (majorClass != 0x04)
        return false;

    switch (minorClass)
    {
        case 0x01:   // Wearable Headset Device
        case 0x02:   // Hands-free Device
        case 0x06:   // Headphones
            return true;

        default:
            return false;
    }
}
BluetoothDevice BluetoothManager::getTargetDevice()
{
    return targetDevice;
}

void BluetoothManager::gapCallback(
    esp_bt_gap_cb_event_t event,
    esp_bt_gap_cb_param_t *param)
{
    switch (event)
    {
        case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
        {
            if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED)
            {
                Serial.println("Discovery Started");
            }
            else if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED)
            {
                Serial.println();
                Serial.println("Discovery Finished");

                Serial.println();
                Serial.println("========== Available Devices ==========");

                BluetoothManager::printDevices();
                if (discoveryFinishedCallback)
{
    discoveryFinishedCallback();
}
                
            }

            break;
        }

        case ESP_BT_GAP_DISC_RES_EVT:
        {
            BluetoothDevice device;

            device.name = "Unknown";
            device.rssi = 0;
            device.cod = 0;

            // Copy MAC address
            memcpy(device.mac, param->disc_res.bda, ESP_BD_ADDR_LEN);

            // Read all properties
            for (int i = 0; i < param->disc_res.num_prop; i++)
            {
                esp_bt_gap_dev_prop_t *prop = &param->disc_res.prop[i];

                switch (prop->type)
                {
                    case ESP_BT_GAP_DEV_PROP_EIR:
                    {
                        uint8_t len = 0;

                        uint8_t *name = esp_bt_gap_resolve_eir_data(
                            (uint8_t *)prop->val,
                            ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME,
                            &len);

                        if (name && len)
                        {
                            char temp[248] = {0};

                            memcpy(temp, name, len);
                            temp[len] = '\0';

                            device.name = String(temp);
                        }

                        break;
                    }

                    case ESP_BT_GAP_DEV_PROP_RSSI:
                    {
                        device.rssi = *(int8_t *)prop->val;
                        break;
                    }

                    case ESP_BT_GAP_DEV_PROP_COD:
                    {
                        device.cod = *(uint32_t *)prop->val;
                        break;
                    }

                    default:
                        break;
                }
            }

            // Remove duplicates
            bool found = false;

            for (BluetoothDevice &d : devices)
            {
                if (memcmp(d.mac, device.mac, ESP_BD_ADDR_LEN) == 0)
                {
                    found = true;

                    if (device.rssi > d.rssi)
                    {
                        d = device;
                    }

                    break;
                }
            }

            if (!found)
            {
                devices.push_back(device);
            }

            break;
        }

        default:
            break;
    }
}
