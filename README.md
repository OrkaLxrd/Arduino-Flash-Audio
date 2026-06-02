# SimpleFlashAudio

<p align="center">
  <strong>Simple Flash Audio</strong><br>
  Tiny RAW audio player for Arduino / ESP / STM32 and external SPI 25 Flash.
</p>

---

## Contents / Оглавление

### [English](#english)

1. [What is SimpleFlashAudio?](#what-is-simpleflashaudio)
2. [Features](#features)
3. [Example API](#example-api)
4. [Planned API](#planned-api)
   - [Playback](#playback)
   - [Volume](#volume)
   - [Flash information](#flash-information)
   - [Audio list](#audio-list)
   - [Uploading audio](#uploading-audio)
   - [Erasing audio](#erasing-audio)
5. [Flash service area](#flash-service-area)
6. [Flash memory format](#flash-memory-format)
7. [Audio format](#audio-format)
8. [Hardware](#hardware)
   - [Required parts](#required-parts)
   - [Tested hardware](#tested-hardware)
   - [Basic Arduino Nano wiring](#basic-arduino-nano-wiring)
9. [Microcontroller support table](#microcontroller-support-table)
10. [Why SPI Flash instead of SD card?](#why-spi-flash-instead-of-sd-card)
11. [Project structure](#project-structure)
12. [Roadmap](#roadmap)
13. [Status](#status)

### [Русский](#русский)

1. [Что такое SimpleFlashAudio?](#что-такое-simpleflashaudio)
2. [Возможности](#возможности)
3. [Пример API](#пример-api)
4. [Планируемый API](#планируемый-api)
   - [Воспроизведение](#воспроизведение)
   - [Громкость](#громкость)
   - [Информация о Flash](#информация-о-flash)
   - [Список аудио](#список-аудио)
   - [Загрузка аудио](#загрузка-аудио)
   - [Стирание аудио](#стирание-аудио)
5. [Служебная область Flash](#служебная-область-flash)
6. [Формат Flash](#формат-flash)
7. [Формат аудио](#формат-аудио)
8. [Железо](#железо)
   - [Нужно](#нужно)
   - [Проверенное железо](#проверенное-железо)
   - [Базовое подключение Arduino Nano](#базовое-подключение-arduino-nano)
9. [Таблица поддержки микроконтроллеров](#таблица-поддержки-микроконтроллеров)
10. [Почему SPI Flash, а не SD-карта?](#почему-spi-flash-а-не-sd-карта)
11. [Структура проекта](#структура-проекта)
12. [Roadmap / планы](#roadmap--планы)
13. [Статус](#статус)
14. [License](#license)
15. [Author](#author)

---

# English

## What is SimpleFlashAudio?

**SimpleFlashAudio** is a lightweight Arduino audio project/library for playing simple 8-bit RAW audio from an external SPI Flash chip.

It is designed for small embedded sound devices where an SD card or MP3 decoder is unnecessary.

Basic idea:

```text
PC → converts audio to RAW unsigned 8-bit PCM
PC / MCU → uploads audio to SPI Flash
Microcontroller → reads audio from Flash
Microcontroller → outputs PWM audio
Amplifier → drives speaker
```

Main tested setup:

```text
Arduino Nano / Uno / Pro Mini
ATmega328P
W25Q SPI Flash
PWM audio output
PAM8403 amplifier
Speaker
```

---

## Features

SimpleFlashAudio is intended to support:

- playing audio from SPI Flash
- simple command: `sflash.play(1);`
- volume control from `1` to `10`
- Flash chip information reading
- manufacturer ID reading
- Flash size detection
- used / free Flash space information
- audio count information
- audio names stored in a service area inside Flash
- uploading audio data to Flash
- erasing one audio track
- erasing several audio tracks
- erasing all audio tracks
- small service directory stored inside Flash
- no SD card required
- no MP3 decoder module required
- no external audio library required
- PWM audio output
- compact format for small devices

---

## Example API

```cpp
#include <SimpleFlashAudio.h>

SimpleFlashAudio sflash;

void setup() {
  Serial.begin(115200);

  sflash.begin(9, 10);      // audio PWM pin, Flash CS pin
  sflash.setVolume(8);      // volume: 1..10

  sflash.play(1);           // play audio number 1
}

void loop() {
  sflash.update();
}
```

---

## Planned API

### Playback

```cpp
sflash.play(1);             // play audio by index
sflash.stop();              // stop playback
sflash.pause();             // pause playback
sflash.resume();            // resume playback
sflash.isPlaying();         // true / false
```

### Volume

```cpp
sflash.setVolume(10);       // volume 1..10
sflash.getVolume();         // current volume
```

### Flash information

```cpp
sflash.getManufacturerId();
sflash.getFlashSize();
sflash.getUsedSize();
sflash.getFreeSize();
sflash.getUsedPercent();
```

Example:

```cpp
Serial.print("Manufacturer ID: ");
Serial.println(sflash.getManufacturerId(), HEX);

Serial.print("Flash size: ");
Serial.println(sflash.getFlashSize());

Serial.print("Used: ");
Serial.println(sflash.getUsedPercent());
```

### Audio list

```cpp
sflash.getAudioCount();
sflash.getAudioName(1);
sflash.getAudioSize(1);
sflash.getAudioDuration(1);
```

Example:

```cpp
uint8_t count = sflash.getAudioCount();

for (uint8_t i = 1; i <= count; i++) {
  Serial.print(i);
  Serial.print(": ");
  Serial.println(sflash.getAudioName(i));
}
```

### Uploading audio

```cpp
sflash.beginUpload();
sflash.writeAudioChunk(data, length);
sflash.endUpload("sound_1", 11025);
```

### Erasing audio

```cpp
sflash.eraseAudio(1);       // erase one audio
sflash.eraseAudio(1, 3);    // erase audio from 1 to 3
sflash.eraseAll();          // erase all audio
```

---

## Flash service area

SimpleFlashAudio stores a small service directory inside the SPI Flash.

This area can contain:

```text
magic signature
format version
Flash size
used bytes
free bytes
audio count
audio names
audio addresses
audio sizes
sample rates
CRC32 checksums
flags
```

This allows the microcontroller to quickly know:

```text
how many audio files are stored
where each audio starts
how large each audio is
what the audio names are
how much Flash is used
how much Flash is free
```

If fast scanning becomes practical later, some stored usage information may become optional.

---

## Flash memory format

Current concept:

```text
0x000000  Service header
0x000010  Audio table
0x001000  Audio data area
```

Example audio table entry:

```text
valid flag
audio index
sample rate
start address
size
CRC32
name
```

The goal is to keep the format simple enough for 8-bit microcontrollers.

---

## Audio format

Recommended audio format:

```text
Unsigned 8-bit PCM
Mono
RAW stream
8000 Hz / 11025 Hz / 16000 Hz
```

The microcontroller does not decode MP3, AAC, OGG, FLAC, or WAV directly during playback.

Audio should be converted on the computer before upload:

```text
MP3 / WAV / OGG / FLAC / M4A → RAW unsigned 8-bit mono PCM
```

---

## Hardware

### Required parts

```text
Microcontroller
W25Q SPI Flash chip
Audio amplifier
Speaker
RC low-pass filter
```

### Tested hardware

```text
Arduino Nano / ATmega328P
W25Q SPI Flash
PAM8403 amplifier
Speaker
```

### Basic Arduino Nano wiring

```text
Arduino Nano D9  → RC filter → amplifier input
Arduino Nano D10 → Flash CS
Arduino Nano D11 → Flash DI / MOSI
Arduino Nano D12 → Flash DO / MISO
Arduino Nano D13 → Flash CLK
3.3V             → Flash VCC
GND              → common GND
```

The W25Q Flash chip must be powered from **3.3V**.

If the microcontroller uses 5V logic, use level shifting or resistor dividers for Flash input pins:

```text
CS
MOSI / DI
SCK / CLK
```

The Flash `DO / MISO` pin can usually go directly to an ATmega328P input.
<p align="center">
  <img src="https://cdn-learn.adafruit.com/assets/assets/000/011/519/medium800/trinket_programming-schematic.png?1381553443" alt="Схема подключения SimpleFlashAudio" width="700">
</p>

---

## Microcontroller support table

| MCU / Board | Status | Notes |
|---|---:|---|
| Arduino Nano / ATmega328P | ✅ | Tested and working |
| Arduino Uno / ATmega328P | ✅ | Same MCU family as Nano |
| Arduino Pro Mini / ATmega328P | ✅ | Same MCU family as Nano |
| Other ATmega328P boards | ✅ | Expected to work with the same pin/timer setup |
| ATmega168 | ❓ | Not tested, less Flash/RAM |
| ATmega32 / ATmega16 | ❓ | Not tested |
| ATmega2560 / Arduino Mega | ❓ | Not tested, timer mapping may differ |
| ATtiny85 | ❓ | Not tested, possible with reduced features |
| ATtiny26 | ❓ | Not tested, likely very limited |
| STM8S003F3P6 | ❓ | Planned, not tested |
| STM32 | ❓ | Planned, not tested |
| ESP8266 | ❓ | Planned, not tested |
| ESP32 | ❓ | Planned, not tested |
| CH32V003 | ❓ | Planned, not tested |
| RP2040 | ❓ | Planned, not tested |

Legend:

```text
✅ tested / working
❓ planned or expected, but not tested
⚠️ partially working or unstable
❌ not supported / not working
```

---

## Why SPI Flash instead of SD card?

SPI Flash is useful for compact sound devices.

Compared to SD cards, SPI Flash is:

```text
smaller
cheap
soldered directly to PCB
fast enough for RAW audio
good for fixed sound sets
simple for embedded products
```

This makes it suitable for toys, alarms, small soundboards, voice modules, and custom embedded devices.

---

## Project structure

Possible project structure:

```text
SimpleFlashAudio/
  src/
    SimpleFlashAudio.h
    SimpleFlashAudio.cpp

  examples/
    BasicPlayer/
      BasicPlayer.ino

    FlashInfo/
      FlashInfo.ino

    UploadExample/
      UploadExample.ino

    EraseAudio/
      EraseAudio.ino

  tools/
    simple_flash_audio_studio.py

  docs/
    wiring.md
    flash_format.md
    audio_format.md
    protocol.md

  README.md
  LICENSE
  library.properties
```

---

## Roadmap

Planned features:

- stable Arduino library structure
- `sflash.play(index)`
- volume control from 1 to 10
- Flash information API
- Flash usage API
- audio list API
- audio names stored in Flash
- PC uploader tool
- Serial upload protocol
- erase one audio
- erase multiple audio files
- erase all audio files
- CRC verification
- support for more W25Q sizes
- support for PROGMEM audio
- STM32 support
- ESP8266 support
- ESP32 support
- STM8 support
- CH32V003 support

---

## Status

The project is designed as a ready-to-use audio solution for Arduino-style microcontrollers.

Current working target:

```text
ATmega328P / Arduino Nano / Uno / Pro Mini
```

Other targets are planned but not tested yet.

---

# Русский

## Что такое SimpleFlashAudio?

**SimpleFlashAudio** — это лёгкий Arduino audio проект/библиотека для воспроизведения простого 8-битного RAW-аудио из внешней SPI Flash-памяти.

Проект рассчитан на маленькие устройства, где SD-карта или MP3-декодер не нужны.

Основная идея:

```text
ПК → конвертирует аудио в RAW unsigned 8-bit PCM
ПК / МК → загружает аудио в SPI Flash
Микроконтроллер → читает аудио из Flash
Микроконтроллер → выдаёт звук через PWM
Усилитель → раскачивает динамик
```

Основная проверенная схема:

```text
Arduino Nano / Uno / Pro Mini
ATmega328P
W25Q SPI Flash
PWM-выход звука
усилитель PAM8403
динамик
```

---

## Возможности

SimpleFlashAudio должен уметь:

- воспроизводить аудио из SPI Flash
- простая команда: `sflash.play(1);`
- регулировать громкость от `1` до `10`
- выводить информацию о Flash
- выводить ID производителя
- определять размер Flash
- показывать занятое и свободное место
- показывать процент заполненности
- выводить количество аудио
- выводить названия аудио
- хранить названия и служебную информацию внутри Flash
- загружать аудио во Flash
- стирать одно аудио
- стирать несколько аудио
- стирать все аудио
- использовать небольшую служебную область памяти
- работать без SD-карты
- работать без MP3-декодера
- работать без обязательных внешних audio-библиотек
- выдавать звук через PWM

---

## Пример API

```cpp
#include <SimpleFlashAudio.h>

SimpleFlashAudio sflash;

void setup() {
  Serial.begin(115200);

  sflash.begin(9, 10);      // PWM-пин звука, CS-пин Flash
  sflash.setVolume(8);      // громкость: 1..10

  sflash.play(1);           // проиграть аудио номер 1
}

void loop() {
  sflash.update();
}
```

---

## Планируемый API

### Воспроизведение

```cpp
sflash.play(1);             // проиграть аудио по номеру
sflash.stop();              // остановить
sflash.pause();             // пауза
sflash.resume();            // продолжить
sflash.isPlaying();         // true / false
```

### Громкость

```cpp
sflash.setVolume(10);       // громкость 1..10
sflash.getVolume();         // текущая громкость
```

### Информация о Flash

```cpp
sflash.getManufacturerId();
sflash.getFlashSize();
sflash.getUsedSize();
sflash.getFreeSize();
sflash.getUsedPercent();
```

Пример:

```cpp
Serial.print("Manufacturer ID: ");
Serial.println(sflash.getManufacturerId(), HEX);

Serial.print("Flash size: ");
Serial.println(sflash.getFlashSize());

Serial.print("Used: ");
Serial.println(sflash.getUsedPercent());
```

### Список аудио

```cpp
sflash.getAudioCount();
sflash.getAudioName(1);
sflash.getAudioSize(1);
sflash.getAudioDuration(1);
```

Пример:

```cpp
uint8_t count = sflash.getAudioCount();

for (uint8_t i = 1; i <= count; i++) {
  Serial.print(i);
  Serial.print(": ");
  Serial.println(sflash.getAudioName(i));
}
```

### Загрузка аудио

```cpp
sflash.beginUpload();
sflash.writeAudioChunk(data, length);
sflash.endUpload("sound_1", 11025);
```

### Стирание аудио

```cpp
sflash.eraseAudio(1);       // стереть одно аудио
sflash.eraseAudio(1, 3);    // стереть аудио с 1 по 3
sflash.eraseAll();          // стереть все аудио
```

---

## Служебная область Flash

SimpleFlashAudio хранит небольшую служебную область внутри SPI Flash.

Там могут храниться:

```text
магическая сигнатура
версия формата
размер Flash
занято байт
свободно байт
количество аудио
названия аудио
адреса аудио
размеры аудио
sample rate
CRC32
флаги
```

Благодаря этому микроконтроллер может быстро узнать:

```text
сколько аудио записано
где начинается каждое аудио
какой размер у каждого аудио
какие названия у аудио
сколько Flash занято
сколько Flash свободно
```

Если позже получится быстро проверять заполненность Flash сканированием, часть этих данных можно будет не хранить.

---

## Формат Flash

Текущая идея:

```text
0x000000  служебный заголовок
0x000010  таблица аудио
0x001000  область аудио-данных
```

Пример записи в таблице аудио:

```text
флаг активности
номер аудио
sample rate
адрес начала
размер
CRC32
название
```

Цель — оставить формат достаточно простым даже для 8-битных микроконтроллеров.

---

## Формат аудио

Рекомендуемый формат:

```text
Unsigned 8-bit PCM
Mono
RAW stream
8000 Hz / 11025 Hz / 16000 Hz
```

Микроконтроллер не декодирует MP3, AAC, OGG, FLAC или WAV во время воспроизведения.

Аудио нужно заранее конвертировать на компьютере:

```text
MP3 / WAV / OGG / FLAC / M4A → RAW unsigned 8-bit mono PCM
```

---

## Железо

### Нужно

```text
Микроконтроллер
SPI Flash W25Q
Аудиоусилитель
Динамик
RC-фильтр
```

### Проверенное железо

```text
Arduino Nano / ATmega328P
SPI Flash W25Q
усилитель PAM8403
динамик
```

### Базовое подключение Arduino Nano

```text
Arduino Nano D9  → RC-фильтр → вход усилителя
Arduino Nano D10 → Flash CS
Arduino Nano D11 → Flash DI / MOSI
Arduino Nano D12 → Flash DO / MISO
Arduino Nano D13 → Flash CLK
3.3V             → Flash VCC
GND              → общий GND
```

Flash W25Q должна питаться от **3.3V**.

Если микроконтроллер работает на 5V логике, для входов Flash нужны делители или level shifter:

```text
CS
MOSI / DI
SCK / CLK
```

Пин `DO / MISO` от Flash обычно можно подключать напрямую ко входу ATmega328P.
<p align="center">
  <img src="https://cdn-learn.adafruit.com/assets/assets/000/011/519/medium800/trinket_programming-schematic.png?1381553443" alt="Схема подключения SimpleFlashAudio" width="700">
</p>

---

## Таблица поддержки микроконтроллеров

| МК / плата | Статус | Примечание |
|---|---:|---|
| Arduino Nano / ATmega328P | ✅ | Проверено, работает |
| Arduino Uno / ATmega328P | ✅ | То же семейство МК, что Nano |
| Arduino Pro Mini / ATmega328P | ✅ | То же семейство МК, что Nano |
| Другие платы ATmega328P | ✅ | Должны работать с тем же таймером и пинами |
| ATmega168 | ❓ | Не проверено, меньше Flash/RAM |
| ATmega32 / ATmega16 | ❓ | Не проверено |
| ATmega2560 / Arduino Mega | ❓ | Не проверено, таймеры и пины могут отличаться |
| ATtiny85 | ❓ | Не проверено, возможно с урезанными функциями |
| ATtiny26 | ❓ | Не проверено, скорее всего очень ограниченно |
| STM8S003F3P6 | ❓ | В планах, не проверено |
| STM32 | ❓ | В планах, не проверено |
| ESP8266 | ❓ | В планах, не проверено |
| ESP32 | ❓ | В планах, не проверено |
| CH32V003 | ❓ | В планах, не проверено |
| RP2040 | ❓ | В планах, не проверено |

Обозначения:

```text
✅ проверено / работает
❓ планируется или ожидается, но не проверено
⚠️ частично работает или нестабильно
❌ не поддерживается / не работает
```

---

## Почему SPI Flash, а не SD-карта?

SPI Flash удобна для компактных звуковых устройств.

По сравнению с SD-картой SPI Flash:

```text
меньше
дешевле
припаивается прямо на плату
достаточно быстрая для RAW-аудио
хорошо подходит для фиксированного набора звуков
удобна для embedded-устройств
```

Это хорошо подходит для игрушек, сигнализаций, маленьких soundboard-модулей, голосовых модулей и кастомных embedded-устройств.

---

## Структура проекта

Возможная структура проекта:

```text
SimpleFlashAudio/
  src/
    SimpleFlashAudio.h
    SimpleFlashAudio.cpp

  examples/
    BasicPlayer/
      BasicPlayer.ino

    FlashInfo/
      FlashInfo.ino

    UploadExample/
      UploadExample.ino

    EraseAudio/
      EraseAudio.ino

  tools/
    simple_flash_audio_studio.py

  docs/
    wiring.md
    flash_format.md
    audio_format.md
    protocol.md

  README.md
  LICENSE
  library.properties
```

---

## Roadmap / планы

Планируемые функции:

- нормальная структура Arduino-библиотеки
- `sflash.play(index)`
- громкость от 1 до 10
- API информации о Flash
- API занятости Flash
- API списка аудио
- хранение названий аудио во Flash
- инструмент загрузки с ПК
- Serial-протокол загрузки
- стирание одного аудио
- стирание нескольких аудио
- стирание всех аудио
- CRC-проверка
- поддержка разных W25Q
- поддержка PROGMEM-аудио
- поддержка STM32
- поддержка ESP8266
- поддержка ESP32
- поддержка STM8
- поддержка CH32V003

---

## Статус

Проект задуман как готовое audio-решение для Arduino-совместимых микроконтроллеров.

Текущая рабочая цель:

```text
ATmega328P / Arduino Nano / Uno / Pro Mini
```

Остальные цели планируются, но пока не проверены.

---

## License

MIT License

---

## Author

Created by Orka Lord.
