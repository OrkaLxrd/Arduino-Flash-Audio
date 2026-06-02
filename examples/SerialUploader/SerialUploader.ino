#include <SimpleFlashAudio.h>

SimpleFlashAudio sflash;

void setup() {
  Serial.begin(115200);

  if (!sflash.begin(9, 10)) {
    Serial.println("SimpleFlashAudio init failed");
    while (true) {
    }
  }

  Serial.println("SFA uploader mode ready");
}

void loop() {
  sflash.update();
  sflash.handleSerial(Serial);
}
