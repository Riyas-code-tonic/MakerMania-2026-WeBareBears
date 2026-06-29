#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

void setup() {
  Serial.begin(115200);

  if (!SerialBT.begin("ESP32")) {
    Serial.println("Bluetooth init failed!");
    return;
  }

  Serial.println("Scanning...");

  BTScanResults *results = SerialBT.discover(10000);

  if (results == nullptr) {
    Serial.println("No devices found.");
    return;
  }

  Serial.printf("Found %d devices\n\n", results->getCount());

for (int i = 0; i < results->getCount(); i++) {

    BTAdvertisedDevice *d = results->getDevice(i);

    Serial.println("------------------------");

    Serial.print("Name : ");
    Serial.println(d->getName().c_str());

    Serial.print("MAC  : ");
    Serial.println(d->getAddress().toString().c_str());

    Serial.print("RSSI : ");
    Serial.println(d->getRSSI());
}

  delete results;
}

void loop() {
}
