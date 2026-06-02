#include <SimpleFlashAudio.h>

SimpleFlashAudio sflash;

void setup() {
  Serial.begin(115200);

  if (!sflash.begin(9, 10)) {
    Serial.println("Init failed");
    while (true) {
    }
  }

  SFAFlashInfo info = sflash.getFlashInfo();

  Serial.print("Manufacturer ID: 0x");
  Serial.println(info.manufacturerId, HEX);

  Serial.print("Memory type: 0x");
  Serial.println(info.memoryType, HEX);

  Serial.print("Capacity ID: 0x");
  Serial.println(info.capacityId, HEX);

  Serial.print("Flash size bytes: ");
  Serial.println(info.sizeBytes);

  Serial.print("Used bytes: ");
  Serial.println(sflash.getUsedSize());

  Serial.print("Free bytes: ");
  Serial.println(sflash.getFreeSize());

  Serial.print("Used percent: ");
  Serial.println(sflash.getUsedPercent());

  Serial.print("Audio count: ");
  Serial.println(sflash.getAudioCount());
}

void loop() {
}
