#include <SimpleFlashAudio.h>

SimpleFlashAudio sflash;

void setup() {
  Serial.begin(115200);

  if (!sflash.begin(9, 10)) {
    Serial.println("SimpleFlashAudio init failed");
    Serial.println("For ATmega328P audio pin must be D9");
    while (true) {
    }
  }

  sflash.setVolume(8);

  Serial.println("SimpleFlashAudio ready");
  Serial.println("Send 1..9 in Serial Monitor to play audio slot");
  Serial.println("Send D for JEDEC ID");
  Serial.println("Send M for flash info");
  Serial.println("Send L for audio list");
}

void loop() {
  sflash.update();
  sflash.handleSerial(Serial);
}
