#ifndef BLUETOOTHMANAGER_H
#define BLUETOOTHMANAGER_H

#include <Arduino.h>
#include <BluetoothSerial.h>
#include <vector>

extern "C"
{
#include "esp_gap_bt_api.h"
#include "esp_bt_device.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
}

struct BluetoothDevice
{
    String name;

    uint8_t mac[6];

    int rssi;

    uint32_t cod;

    bool operator==(const BluetoothDevice &other) const
    {
        return memcmp(mac, other.mac, 6) == 0;
    }
};

class BluetoothManager
{
    
public:

bool foundDevice();

static void setDiscoveryFinishedCallback(void (*callback)());


    BluetoothManager();

    bool begin();

    void startDiscovery(uint8_t duration);

    void stopDiscovery();

    static void printDevices();

    static String getDeviceType(uint32_t cod);
    static bool isTargetAudioDevice(uint32_t cod);
    static BluetoothDevice getTargetDevice();

private:




    BluetoothSerial serialBT;

    static void gapCallback(
        esp_bt_gap_cb_event_t event,
        esp_bt_gap_cb_param_t *param);

    static std::vector<BluetoothDevice> devices;
    static BluetoothDevice targetDevice;
    static void (*discoveryFinishedCallback)();
};

#endif