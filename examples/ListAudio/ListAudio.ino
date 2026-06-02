#include <SimpleFlashAudio.h>

SimpleFlashAudio sflash;

void setup() {
  Serial.begin(115200);

  if (!sflash.begin(9, 10)) {
    Serial.println("Init failed");
    while (true) {
    }
  }

  uint8_t count = sflash.getAudioCount();

  Serial.print("Audio count: ");
  Serial.println(count);

  for (uint8_t i = 1; i <= count; i++) {
    SFAAudioInfo info;

    if (sflash.getAudioInfo(i, info)) {
      Serial.print(i);
      Serial.print(": ");
      Serial.print(info.name);
      Serial.print(" | ");
      Serial.print(info.sampleRate);
      Serial.print(" Hz | ");
      Serial.print(info.size);
      Serial.println(" bytes");
    }
  }
}

void loop() {
}
