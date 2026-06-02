#ifndef SIMPLE_FLASH_AUDIO_H
#define SIMPLE_FLASH_AUDIO_H

#include <Arduino.h>
#include <SPI.h>

#define SFA_VERSION_MAJOR 0
#define SFA_VERSION_MINOR 1
#define SFA_VERSION_PATCH 0

#define SFA_MAGIC_0 'S'
#define SFA_MAGIC_1 'F'
#define SFA_MAGIC_2 'A'
#define SFA_MAGIC_3 '1'

#define SFA_LEGACY_MAGIC_0 'N'
#define SFA_LEGACY_MAGIC_1 'S'
#define SFA_LEGACY_MAGIC_2 'F'
#define SFA_LEGACY_MAGIC_3 'X'

#define SFA_FORMAT_VERSION 1

#define SFA_DEFAULT_DATA_START 0x001000UL
#define SFA_HEADER_ADDRESS     0x000000UL
#define SFA_HEADER_SIZE        64UL
#define SFA_ENTRY_SIZE         64UL
#define SFA_MAX_TRACKS         32

#define SFA_LEGACY_HEADER_SIZE 16UL
#define SFA_LEGACY_ENTRY_SIZE  32UL
#define SFA_LEGACY_MAX_TRACKS  16
#define SFA_LEGACY_DATA_START  0x001000UL

#define SFA_PAGE_SIZE          256UL
#define SFA_SECTOR_SIZE        4096UL

#define SFA_AUDIO_BUFFER_SIZE  512
#define SFA_READ_CHUNK_SIZE    64

#define SFA_CMD_HELLO          'H'
#define SFA_CMD_READ_ID        'I'
#define SFA_CMD_DEBUG_ID       'D'
#define SFA_CMD_ERASE_RANGE    'A'
#define SFA_CMD_WRITE          'W'
#define SFA_CMD_READ           'R'
#define SFA_CMD_PLAY           'P'
#define SFA_CMD_STOP           'S'
#define SFA_CMD_ERASE_ALL      'E'
#define SFA_CMD_FORMAT         'F'
#define SFA_CMD_INFO           'M'
#define SFA_CMD_LIST           'L'

struct SFAFlashInfo {
  uint8_t manufacturerId;
  uint8_t memoryType;
  uint8_t capacityId;
  uint32_t sizeBytes;
};

struct SFAAudioInfo {
  bool valid;
  uint8_t index;
  uint32_t sampleRate;
  uint32_t address;
  uint32_t size;
  uint32_t crc32;
  char name[32];
};

class SimpleFlashAudio {
public:
  SimpleFlashAudio();

  bool begin(uint8_t audioPin, uint8_t flashCsPin, uint32_t spiSpeed = 8000000UL);

  bool play(uint8_t index);
  void stop();
  bool isPlaying();
  void update();

  void setVolume(uint8_t volume);
  uint8_t getVolume();

  SFAFlashInfo getFlashInfo();
  uint8_t getManufacturerId();
  uint8_t getMemoryType();
  uint8_t getCapacityId();
  uint32_t getFlashSize();

  uint32_t getUsedSize();
  uint32_t getFreeSize();
  uint8_t getUsedPercent();

  uint8_t getAudioCount();
  bool getAudioInfo(uint8_t index, SFAAudioInfo &info);
  String getAudioName(uint8_t index);
  uint32_t getAudioSize(uint8_t index);
  uint32_t getAudioDurationMs(uint8_t index);

  bool format();
  bool eraseAudio(uint8_t index);
  bool eraseAudio(uint8_t firstIndex, uint8_t lastIndex);
  bool eraseAll();

  bool eraseRange(uint32_t address, uint32_t length);
  bool writeBytes(uint32_t address, const uint8_t *data, uint16_t length);
  bool readBytes(uint32_t address, uint8_t *data, uint16_t length);

  bool handleSerial(Stream &stream);

  static SimpleFlashAudio *activeInstance;
  static void isrSample();

private:
  uint8_t _audioPin;
  uint8_t _flashCsPin;
  uint32_t _spiSpeed;
  uint8_t _volume;

  volatile uint8_t _audioBuffer[SFA_AUDIO_BUFFER_SIZE];
  volatile uint16_t _audioHead;
  volatile uint16_t _audioTail;
  volatile uint16_t _audioCount;

  volatile bool _playing;
  volatile bool _playbackEnded;

  uint32_t _currentReadAddress;
  uint32_t _currentReadRemaining;
  uint8_t _tempReadBuffer[SFA_READ_CHUNK_SIZE];

  bool _legacyMode;

  void flashSelect();
  void flashDeselect();
  uint8_t flashTransfer(uint8_t data);
  void flashSendAddress(uint32_t address);

  void flashWriteEnable();
  uint8_t flashReadStatus1();
  void flashWaitBusy();
  void flashReadId(uint8_t *manufacturer, uint8_t *memoryType, uint8_t *capacity);
  void flashRead(uint32_t address, uint8_t *buffer, uint16_t length);
  void flashPageProgramRaw(uint32_t address, const uint8_t *data, uint16_t length);
  void flashSectorErase(uint32_t address);

  bool setupPwmAudio();
  bool setupSampleTimer(uint32_t sampleRate);
  void stopSampleTimer();

  void clearAudioBuffer();
  void pushAudioByte(uint8_t value);
  void fillAudioBuffer();
  void startPlayback(uint32_t address, uint32_t length, uint32_t sampleRate);

  bool readTrackEntry(uint8_t index, SFAAudioInfo &info);
  bool readModernTrackEntry(uint8_t index, SFAAudioInfo &info);
  bool readLegacyTrackEntry(uint8_t index, SFAAudioInfo &info);

  bool hasModernHeader();
  bool hasLegacyHeader();

  uint16_t readUint16FromBuffer(const uint8_t *buffer, uint8_t offset);
  uint32_t readUint32FromBuffer(const uint8_t *buffer, uint8_t offset);
  void writeUint16ToBuffer(uint8_t *buffer, uint8_t offset, uint16_t value);
  void writeUint32ToBuffer(uint8_t *buffer, uint8_t offset, uint32_t value);

  bool serialReadExact(Stream &stream, uint8_t *buffer, uint16_t length, uint16_t timeoutMs = 3000);
  uint32_t serialReadUint32LE(Stream &stream, bool &ok);
  uint16_t serialReadUint16LE(Stream &stream, bool &ok);
  void serialSendOk(Stream &stream);
  void serialSendErr(Stream &stream);
  void serialPrintInfo(Stream &stream);
  void serialPrintList(Stream &stream);
};

#endif
