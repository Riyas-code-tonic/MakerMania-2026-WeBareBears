
bool discoveryFinished = false;
#include "BluetoothManager.h"
#include "A2DPSourceManager.h"

BluetoothManager btManager;
A2DPSourceManager a2dpManager;


void onDiscoveryFinished()
{
    a2dpManager.connect();
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    if (!btManager.begin())
    {
        Serial.println("Bluetooth Manager initialization failed!");
        return;
    }
    BluetoothManager::setDiscoveryFinishedCallback(onDiscoveryFinished);

    if (!a2dpManager.begin())
    {
        Serial.println("A2DP Manager initialization failed!");
        return;
    }

    delay(1000);
    btManager.startDiscovery(8);
    
}
void loop()
{
    if (discoveryFinished)
    {
        discoveryFinished = false;
        a2dpManager.connect();
    }
    

    // Wait for GAP callback events
}