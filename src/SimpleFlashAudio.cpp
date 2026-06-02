#include "SimpleFlashAudio.h"

#define W25_CMD_WRITE_ENABLE  0x06
#define W25_CMD_READ_STATUS1  0x05
#define W25_CMD_READ_DATA     0x03
#define W25_CMD_PAGE_PROGRAM  0x02
#define W25_CMD_SECTOR_ERASE  0x20
#define W25_CMD_JEDEC_ID      0x9F

SimpleFlashAudio *SimpleFlashAudio::activeInstance = NULL;

SimpleFlashAudio::SimpleFlashAudio() {
  _audioPin = 255;
  _flashCsPin = 255;
  _spiSpeed = 8000000UL;
  _volume = 10;

  _audioHead = 0;
  _audioTail = 0;
  _audioCount = 0;

  _playing = false;
  _playbackEnded = false;

  _currentReadAddress = 0;
  _currentReadRemaining = 0;

  _legacyMode = false;
}

bool SimpleFlashAudio::begin(uint8_t audioPin, uint8_t flashCsPin, uint32_t spiSpeed) {
  _audioPin = audioPin;
  _flashCsPin = flashCsPin;
  _spiSpeed = spiSpeed;

  pinMode(_flashCsPin, OUTPUT);
  digitalWrite(_flashCsPin, HIGH);

  SPI.begin();
  SPI.beginTransaction(SPISettings(_spiSpeed, MSBFIRST, SPI_MODE0));

  if (!setupPwmAudio()) {
    return false;
  }

  stopSampleTimer();
  clearAudioBuffer();

  activeInstance = this;

  _legacyMode = false;

  if (hasLegacyHeader()) {
    _legacyMode = true;
  }

  return true;
}

bool SimpleFlashAudio::play(uint8_t index) {
  SFAAudioInfo info;

  if (!readTrackEntry(index, info)) {
    return false;
  }

  startPlayback(info.address, info.size, info.sampleRate);
  return true;
}

void SimpleFlashAudio::stop() {
  _playing = false;
  _playbackEnded = true;

  stopSampleTimer();
  clearAudioBuffer();

#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
  OCR1A = 128;
#endif
}

bool SimpleFlashAudio::isPlaying() {
  return _playing;
}

void SimpleFlashAudio::update() {
  if (_playing) {
    fillAudioBuffer();
  }

  if (_playbackEnded) {
    stopSampleTimer();
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
    OCR1A = 128;
#endif
  }
}

void SimpleFlashAudio::setVolume(uint8_t volume) {
  if (volume < 1) {
    volume = 1;
  }

  if (volume > 10) {
    volume = 10;
  }

  _volume = volume;
}

uint8_t SimpleFlashAudio::getVolume() {
  return _volume;
}

SFAFlashInfo SimpleFlashAudio::getFlashInfo() {
  SFAFlashInfo info;

  info.manufacturerId = 0;
  info.memoryType = 0;
  info.capacityId = 0;
  info.sizeBytes = 0;

  flashReadId(&info.manufacturerId, &info.memoryType, &info.capacityId);

  if (info.capacityId >= 10 && info.capacityId <= 31) {
    info.sizeBytes = 1UL << info.capacityId;
  }

  return info;
}

uint8_t SimpleFlashAudio::getManufacturerId() {
  return getFlashInfo().manufacturerId;
}

uint8_t SimpleFlashAudio::getMemoryType() {
  return getFlashInfo().memoryType;
}

uint8_t SimpleFlashAudio::getCapacityId() {
  return getFlashInfo().capacityId;
}

uint32_t SimpleFlashAudio::getFlashSize() {
  return getFlashInfo().sizeBytes;
}

uint32_t SimpleFlashAudio::getUsedSize() {
  uint32_t used = SFA_DEFAULT_DATA_START;

  uint8_t count = getAudioCount();

  for (uint8_t i = 1; i <= count; i++) {
    SFAAudioInfo info;

    if (getAudioInfo(i, info)) {
      uint32_t endAddress = info.address + info.size;

      if (endAddress > used) {
        used = endAddress;
      }
    }
  }

  if (used % SFA_SECTOR_SIZE != 0) {
    used = ((used / SFA_SECTOR_SIZE) + 1UL) * SFA_SECTOR_SIZE;
  }

  return used;
}

uint32_t SimpleFlashAudio::getFreeSize() {
  uint32_t flashSize = getFlashSize();
  uint32_t used = getUsedSize();

  if (flashSize <= used) {
    return 0;
  }

  return flashSize - used;
}

uint8_t SimpleFlashAudio::getUsedPercent() {
  uint32_t flashSize = getFlashSize();

  if (flashSize == 0) {
    return 0;
  }

  uint32_t used = getUsedSize();

  if (used >= flashSize) {
    return 100;
  }

  return (uint8_t)((used * 100UL) / flashSize);
}

uint8_t SimpleFlashAudio::getAudioCount() {
  uint8_t count = 0;

  uint8_t maxTracks = _legacyMode ? SFA_LEGACY_MAX_TRACKS : SFA_MAX_TRACKS;

  for (uint8_t i = 1; i <= maxTracks; i++) {
    SFAAudioInfo info;

    if (getAudioInfo(i, info)) {
      count++;
    }
  }

  return count;
}

bool SimpleFlashAudio::getAudioInfo(uint8_t index, SFAAudioInfo &info) {
  return readTrackEntry(index, info);
}

String SimpleFlashAudio::getAudioName(uint8_t index) {
  SFAAudioInfo info;

  if (!getAudioInfo(index, info)) {
    return String("");
  }

  return String(info.name);
}

uint32_t SimpleFlashAudio::getAudioSize(uint8_t index) {
  SFAAudioInfo info;

  if (!getAudioInfo(index, info)) {
    return 0;
  }

  return info.size;
}

uint32_t SimpleFlashAudio::getAudioDurationMs(uint8_t index) {
  SFAAudioInfo info;

  if (!getAudioInfo(index, info)) {
    return 0;
  }

  if (info.sampleRate == 0) {
    return 0;
  }

  return (info.size * 1000UL) / info.sampleRate;
}

bool SimpleFlashAudio::format() {
  stop();

  uint8_t sector[SFA_SECTOR_SIZE];

  for (uint16_t i = 0; i < SFA_SECTOR_SIZE; i++) {
    sector[i] = 0xFF;
  }

  sector[0] = SFA_MAGIC_0;
  sector[1] = SFA_MAGIC_1;
  sector[2] = SFA_MAGIC_2;
  sector[3] = SFA_MAGIC_3;

  writeUint16ToBuffer(sector, 4, SFA_FORMAT_VERSION);
  writeUint16ToBuffer(sector, 6, SFA_MAX_TRACKS);
  writeUint32ToBuffer(sector, 8, SFA_DEFAULT_DATA_START);
  writeUint32ToBuffer(sector, 12, getFlashSize());
  writeUint32ToBuffer(sector, 16, SFA_DEFAULT_DATA_START);
  writeUint16ToBuffer(sector, 20, 0);

  eraseRange(0, SFA_SECTOR_SIZE);

  for (uint16_t offset = 0; offset < SFA_SECTOR_SIZE; offset += SFA_PAGE_SIZE) {
    writeBytes(offset, sector + offset, SFA_PAGE_SIZE);
  }

  _legacyMode = false;
  return true;
}

bool SimpleFlashAudio::eraseAudio(uint8_t index) {
  return eraseAudio(index, index);
}

bool SimpleFlashAudio::eraseAudio(uint8_t firstIndex, uint8_t lastIndex) {
  if (firstIndex < 1) {
    firstIndex = 1;
  }

  if (lastIndex < firstIndex) {
    return false;
  }

  stop();

  for (uint8_t index = firstIndex; index <= lastIndex; index++) {
    SFAAudioInfo info;

    if (!getAudioInfo(index, info)) {
      continue;
    }

    eraseRange(info.address, info.size);

    if (!_legacyMode) {
      uint32_t entryAddress = SFA_HEADER_SIZE + ((uint32_t)(index - 1) * SFA_ENTRY_SIZE);
      uint8_t emptyEntry[SFA_ENTRY_SIZE];

      for (uint8_t i = 0; i < SFA_ENTRY_SIZE; i++) {
        emptyEntry[i] = 0xFF;
      }

      eraseRange(entryAddress, SFA_ENTRY_SIZE);
      writeBytes(entryAddress, emptyEntry, SFA_ENTRY_SIZE);
    }
  }

  return true;
}

bool SimpleFlashAudio::eraseAll() {
  uint32_t flashSize = getFlashSize();

  if (flashSize == 0) {
    return false;
  }

  stop();
  return eraseRange(0, flashSize);
}

bool SimpleFlashAudio::eraseRange(uint32_t address, uint32_t length) {
  if (length == 0) {
    return true;
  }

  uint32_t endAddress = address + length;
  uint32_t sectorAddress = address - (address % SFA_SECTOR_SIZE);

  while (sectorAddress < endAddress) {
    flashSectorErase(sectorAddress);
    sectorAddress += SFA_SECTOR_SIZE;
  }

  return true;
}

bool SimpleFlashAudio::writeBytes(uint32_t address, const uint8_t *data, uint16_t length) {
  if (length == 0) {
    return true;
  }

  uint16_t written = 0;

  while (written < length) {
    uint32_t currentAddress = address + written;
    uint16_t pageOffset = currentAddress % SFA_PAGE_SIZE;
    uint16_t pageRemain = SFA_PAGE_SIZE - pageOffset;
    uint16_t remain = length - written;
    uint16_t chunk = remain;

    if (chunk > pageRemain) {
      chunk = pageRemain;
    }

    flashPageProgramRaw(currentAddress, data + written, chunk);
    written += chunk;
  }

  return true;
}

bool SimpleFlashAudio::readBytes(uint32_t address, uint8_t *data, uint16_t length) {
  if (length == 0) {
    return true;
  }

  flashRead(address, data, length);
  return true;
}

bool SimpleFlashAudio::handleSerial(Stream &stream) {
  if (!stream.available()) {
    return false;
  }

  uint8_t command = stream.read();

  if (command == '\r' || command == '\n' || command == ' ' || command == '\t') {
    return true;
  }

  if (command == SFA_CMD_HELLO) {
    stream.print("SIMPLE_FLASH_AUDIO_V1\n");
    return true;
  }

  if (command == SFA_CMD_READ_ID) {
    SFAFlashInfo info = getFlashInfo();

    stream.write(info.manufacturerId);
    stream.write(info.memoryType);
    stream.write(info.capacityId);
    return true;
  }

  if (command == SFA_CMD_DEBUG_ID) {
    SFAFlashInfo info = getFlashInfo();

    stream.print("JEDEC: 0x");
    if (info.manufacturerId < 16) stream.print("0");
    stream.print(info.manufacturerId, HEX);

    stream.print(" 0x");
    if (info.memoryType < 16) stream.print("0");
    stream.print(info.memoryType, HEX);

    stream.print(" 0x");
    if (info.capacityId < 16) stream.print("0");
    stream.println(info.capacityId, HEX);

    stream.print("SIZE: ");
    stream.println(info.sizeBytes);
    return true;
  }

  if (command == SFA_CMD_ERASE_RANGE) {
    bool ok = true;
    uint32_t address = serialReadUint32LE(stream, ok);
    uint32_t length = serialReadUint32LE(stream, ok);

    if (!ok) {
      serialSendErr(stream);
      return true;
    }

    eraseRange(address, length);
    serialSendOk(stream);
    return true;
  }

  if (command == SFA_CMD_WRITE) {
    bool ok = true;
    uint32_t address = serialReadUint32LE(stream, ok);
    uint16_t length = serialReadUint16LE(stream, ok);

    if (!ok || length == 0 || length > SFA_PAGE_SIZE) {
      serialSendErr(stream);
      return true;
    }

    uint8_t buffer[SFA_PAGE_SIZE];

    if (!serialReadExact(stream, buffer, length)) {
      serialSendErr(stream);
      return true;
    }

    writeBytes(address, buffer, length);
    serialSendOk(stream);
    return true;
  }

  if (command == SFA_CMD_READ) {
    bool ok = true;
    uint32_t address = serialReadUint32LE(stream, ok);
    uint16_t length = serialReadUint16LE(stream, ok);

    if (!ok || length == 0 || length > SFA_PAGE_SIZE) {
      serialSendErr(stream);
      return true;
    }

    uint8_t buffer[SFA_PAGE_SIZE];
    readBytes(address, buffer, length);
    stream.write(buffer, length);
    return true;
  }

  if (command == SFA_CMD_PLAY) {
    uint8_t index = 0;

    if (!serialReadExact(stream, &index, 1)) {
      serialSendErr(stream);
      return true;
    }

    if (!play(index)) {
      serialSendErr(stream);
      return true;
    }

    serialSendOk(stream);
    return true;
  }

  if (command == SFA_CMD_STOP) {
    stop();
    serialSendOk(stream);
    return true;
  }

  if (command == SFA_CMD_ERASE_ALL) {
    if (!eraseAll()) {
      serialSendErr(stream);
      return true;
    }

    serialSendOk(stream);
    return true;
  }

  if (command == SFA_CMD_FORMAT) {
    if (!format()) {
      serialSendErr(stream);
      return true;
    }

    serialSendOk(stream);
    return true;
  }

  if (command == SFA_CMD_INFO) {
    serialPrintInfo(stream);
    return true;
  }

  if (command == SFA_CMD_LIST) {
    serialPrintList(stream);
    return true;
  }

  if (command >= '1' && command <= '9') {
    uint8_t index = command - '0';

    if (play(index)) {
      stream.print("PLAY ");
      stream.println(index);
    } else {
      stream.print("NO AUDIO ");
      stream.println(index);
    }

    return true;
  }

  return false;
}

void SimpleFlashAudio::flashSelect() {
  digitalWrite(_flashCsPin, LOW);
}

void SimpleFlashAudio::flashDeselect() {
  digitalWrite(_flashCsPin, HIGH);
}

uint8_t SimpleFlashAudio::flashTransfer(uint8_t data) {
  return SPI.transfer(data);
}

void SimpleFlashAudio::flashSendAddress(uint32_t address) {
  flashTransfer((address >> 16) & 0xFF);
  flashTransfer((address >> 8) & 0xFF);
  flashTransfer(address & 0xFF);
}

void SimpleFlashAudio::flashWriteEnable() {
  flashSelect();
  flashTransfer(W25_CMD_WRITE_ENABLE);
  flashDeselect();
}

uint8_t SimpleFlashAudio::flashReadStatus1() {
  flashSelect();
  flashTransfer(W25_CMD_READ_STATUS1);
  uint8_t status = flashTransfer(0x00);
  flashDeselect();

  return status;
}

void SimpleFlashAudio::flashWaitBusy() {
  while (flashReadStatus1() & 0x01) {
    delay(1);
  }
}

void SimpleFlashAudio::flashReadId(uint8_t *manufacturer, uint8_t *memoryType, uint8_t *capacity) {
  flashSelect();
  flashTransfer(W25_CMD_JEDEC_ID);

  *manufacturer = flashTransfer(0x00);
  *memoryType = flashTransfer(0x00);
  *capacity = flashTransfer(0x00);

  flashDeselect();
}

void SimpleFlashAudio::flashRead(uint32_t address, uint8_t *buffer, uint16_t length) {
  flashSelect();

  flashTransfer(W25_CMD_READ_DATA);
  flashSendAddress(address);

  for (uint16_t i = 0; i < length; i++) {
    buffer[i] = flashTransfer(0x00);
  }

  flashDeselect();
}

void SimpleFlashAudio::flashPageProgramRaw(uint32_t address, const uint8_t *data, uint16_t length) {
  flashWriteEnable();

  flashSelect();
  flashTransfer(W25_CMD_PAGE_PROGRAM);
  flashSendAddress(address);

  for (uint16_t i = 0; i < length; i++) {
    flashTransfer(data[i]);
  }

  flashDeselect();

  flashWaitBusy();
}

void SimpleFlashAudio::flashSectorErase(uint32_t address) {
  flashWriteEnable();

  flashSelect();
  flashTransfer(W25_CMD_SECTOR_ERASE);
  flashSendAddress(address);
  flashDeselect();

  flashWaitBusy();
}

bool SimpleFlashAudio::setupPwmAudio() {
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
  if (_audioPin != 9) {
    return false;
  }

  pinMode(_audioPin, OUTPUT);

  TCCR1A = 0;
  TCCR1B = 0;

  TCCR1A |= (1 << COM1A1);
  TCCR1A |= (1 << WGM10);

  TCCR1B |= (1 << WGM12);
  TCCR1B |= (1 << CS10);

  OCR1A = 128;

  return true;
#else
  return false;
#endif
}

void SimpleFlashAudio::stopSampleTimer() {
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
  TIMSK2 &= ~(1 << OCIE2A);
  OCR1A = 128;
#endif
}

bool SimpleFlashAudio::setupSampleTimer(uint32_t sampleRate) {
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
  uint32_t ocr = (F_CPU / 8UL / sampleRate) - 1UL;

  if (ocr > 255) {
    return false;
  }

  cli();

  TCCR2A = 0;
  TCCR2B = 0;
  TCNT2 = 0;

  TCCR2A |= (1 << WGM21);
  TCCR2B |= (1 << CS21);

  OCR2A = (uint8_t)ocr;
  TIMSK2 |= (1 << OCIE2A);

  sei();

  return true;
#else
  return false;
#endif
}

void SimpleFlashAudio::clearAudioBuffer() {
  cli();

  _audioHead = 0;
  _audioTail = 0;
  _audioCount = 0;

  sei();
}

void SimpleFlashAudio::pushAudioByte(uint8_t value) {
  bool pushed = false;

  while (!pushed) {
    cli();

    if (_audioCount < SFA_AUDIO_BUFFER_SIZE) {
      _audioBuffer[_audioHead] = value;
      _audioHead++;

      if (_audioHead >= SFA_AUDIO_BUFFER_SIZE) {
        _audioHead = 0;
      }

      _audioCount++;
      pushed = true;
    }

    sei();
  }
}

void SimpleFlashAudio::fillAudioBuffer() {
  if (!_playing) {
    return;
  }

  if (_currentReadRemaining == 0) {
    return;
  }

  while (_currentReadRemaining > 0) {
    uint16_t freeSpace = 0;

    cli();
    freeSpace = SFA_AUDIO_BUFFER_SIZE - _audioCount;
    sei();

    if (freeSpace < SFA_READ_CHUNK_SIZE) {
      return;
    }

    uint16_t chunk = SFA_READ_CHUNK_SIZE;

    if (_currentReadRemaining < chunk) {
      chunk = _currentReadRemaining;
    }

    flashRead(_currentReadAddress, _tempReadBuffer, chunk);

    for (uint16_t i = 0; i < chunk; i++) {
      pushAudioByte(_tempReadBuffer[i]);
    }

    _currentReadAddress += chunk;
    _currentReadRemaining -= chunk;
  }
}

void SimpleFlashAudio::startPlayback(uint32_t address, uint32_t length, uint32_t sampleRate) {
  stopSampleTimer();
  clearAudioBuffer();

  _currentReadAddress = address;
  _currentReadRemaining = length;

  _playing = true;
  _playbackEnded = false;

  fillAudioBuffer();
  fillAudioBuffer();
  fillAudioBuffer();

  if (!setupSampleTimer(sampleRate)) {
    _playing = false;
    _playbackEnded = true;
    stopSampleTimer();
  }
}

void SimpleFlashAudio::isrSample() {
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
  SimpleFlashAudio *a = activeInstance;

  if (a == NULL) {
    OCR1A = 128;
    return;
  }

  if (!a->_playing) {
    OCR1A = 128;
    return;
  }

  if (a->_audioCount > 0) {
    uint8_t sample = a->_audioBuffer[a->_audioTail];

    a->_audioTail++;

    if (a->_audioTail >= SFA_AUDIO_BUFFER_SIZE) {
      a->_audioTail = 0;
    }

    a->_audioCount--;

    int16_t centered = (int16_t)sample - 128;
    centered = (centered * (int16_t)a->_volume) / 10;

    if (centered > 127) {
      centered = 127;
    }

    if (centered < -128) {
      centered = -128;
    }

    OCR1A = (uint8_t)(centered + 128);
  } else {
    OCR1A = 128;

    if (a->_currentReadRemaining == 0) {
      a->_playing = false;
      a->_playbackEnded = true;
    }
  }
#endif
}

#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
ISR(TIMER2_COMPA_vect) {
  SimpleFlashAudio::isrSample();
}
#endif

bool SimpleFlashAudio::readTrackEntry(uint8_t index, SFAAudioInfo &info) {
  if (index < 1) {
    return false;
  }

  if (hasModernHeader()) {
    _legacyMode = false;
    return readModernTrackEntry(index, info);
  }

  if (hasLegacyHeader()) {
    _legacyMode = true;
    return readLegacyTrackEntry(index, info);
  }

  return false;
}

bool SimpleFlashAudio::readModernTrackEntry(uint8_t index, SFAAudioInfo &info) {
  if (index < 1 || index > SFA_MAX_TRACKS) {
    return false;
  }

  uint32_t entryAddress = SFA_HEADER_SIZE + ((uint32_t)(index - 1) * SFA_ENTRY_SIZE);
  uint8_t entry[SFA_ENTRY_SIZE];

  flashRead(entryAddress, entry, SFA_ENTRY_SIZE);

  if (entry[0] != 1) {
    return false;
  }

  info.valid = true;
  info.index = index;
  info.sampleRate = readUint32FromBuffer(entry, 2);
  info.address = readUint32FromBuffer(entry, 6);
  info.size = readUint32FromBuffer(entry, 10);
  info.crc32 = readUint32FromBuffer(entry, 14);

  for (uint8_t i = 0; i < 31; i++) {
    info.name[i] = (char)entry[18 + i];

    if (info.name[i] == 0xFF) {
      info.name[i] = '\0';
    }
  }

  info.name[31] = '\0';

  if (info.address < SFA_DEFAULT_DATA_START) {
    return false;
  }

  if (info.size == 0) {
    return false;
  }

  if (info.sampleRate < 4000 || info.sampleRate > 44100) {
    return false;
  }

  return true;
}

bool SimpleFlashAudio::readLegacyTrackEntry(uint8_t index, SFAAudioInfo &info) {
  if (index < 1 || index > SFA_LEGACY_MAX_TRACKS) {
    return false;
  }

  uint8_t slotIndex = index - 1;
  uint32_t entryAddress = SFA_LEGACY_HEADER_SIZE + ((uint32_t)slotIndex * SFA_LEGACY_ENTRY_SIZE);
  uint8_t entry[SFA_LEGACY_ENTRY_SIZE];

  flashRead(entryAddress, entry, SFA_LEGACY_ENTRY_SIZE);

  if (entry[0] != 1) {
    return false;
  }

  info.valid = true;
  info.index = index;
  info.sampleRate = readUint32FromBuffer(entry, 2);
  info.address = readUint32FromBuffer(entry, 6);
  info.size = readUint32FromBuffer(entry, 10);
  info.crc32 = readUint32FromBuffer(entry, 14);

  for (uint8_t i = 0; i < 14; i++) {
    info.name[i] = (char)entry[18 + i];

    if (info.name[i] == 0xFF) {
      info.name[i] = '\0';
    }
  }

  info.name[14] = '\0';

  if (info.address < SFA_LEGACY_DATA_START) {
    return false;
  }

  if (info.size == 0) {
    return false;
  }

  if (info.sampleRate < 4000 || info.sampleRate > 22050) {
    return false;
  }

  return true;
}

bool SimpleFlashAudio::hasModernHeader() {
  uint8_t header[8];
  flashRead(SFA_HEADER_ADDRESS, header, 8);

  if (
    header[0] == SFA_MAGIC_0 &&
    header[1] == SFA_MAGIC_1 &&
    header[2] == SFA_MAGIC_2 &&
    header[3] == SFA_MAGIC_3
  ) {
    return true;
  }

  return false;
}

bool SimpleFlashAudio::hasLegacyHeader() {
  uint8_t header[8];
  flashRead(SFA_HEADER_ADDRESS, header, 8);

  if (
    header[0] == SFA_LEGACY_MAGIC_0 &&
    header[1] == SFA_LEGACY_MAGIC_1 &&
    header[2] == SFA_LEGACY_MAGIC_2 &&
    header[3] == SFA_LEGACY_MAGIC_3
  ) {
    return true;
  }

  return false;
}

uint16_t SimpleFlashAudio::readUint16FromBuffer(const uint8_t *buffer, uint8_t offset) {
  uint16_t value = 0;
  value |= ((uint16_t)buffer[offset]);
  value |= ((uint16_t)buffer[offset + 1]) << 8;
  return value;
}

uint32_t SimpleFlashAudio::readUint32FromBuffer(const uint8_t *buffer, uint8_t offset) {
  uint32_t value = 0;
  value |= ((uint32_t)buffer[offset]);
  value |= ((uint32_t)buffer[offset + 1]) << 8;
  value |= ((uint32_t)buffer[offset + 2]) << 16;
  value |= ((uint32_t)buffer[offset + 3]) << 24;
  return value;
}

void SimpleFlashAudio::writeUint16ToBuffer(uint8_t *buffer, uint8_t offset, uint16_t value) {
  buffer[offset] = value & 0xFF;
  buffer[offset + 1] = (value >> 8) & 0xFF;
}

void SimpleFlashAudio::writeUint32ToBuffer(uint8_t *buffer, uint8_t offset, uint32_t value) {
  buffer[offset] = value & 0xFF;
  buffer[offset + 1] = (value >> 8) & 0xFF;
  buffer[offset + 2] = (value >> 16) & 0xFF;
  buffer[offset + 3] = (value >> 24) & 0xFF;
}

bool SimpleFlashAudio::serialReadExact(Stream &stream, uint8_t *buffer, uint16_t length, uint16_t timeoutMs) {
  uint16_t received = 0;
  unsigned long startTime = millis();

  while (received < length) {
    if (stream.available()) {
      buffer[received] = stream.read();
      received++;
      startTime = millis();
    }

    if (millis() - startTime > timeoutMs) {
      return false;
    }
  }

  return true;
}

uint32_t SimpleFlashAudio::serialReadUint32LE(Stream &stream, bool &ok) {
  uint8_t b[4];

  if (!serialReadExact(stream, b, 4)) {
    ok = false;
    return 0;
  }

  uint32_t value = 0;
  value |= ((uint32_t)b[0]);
  value |= ((uint32_t)b[1]) << 8;
  value |= ((uint32_t)b[2]) << 16;
  value |= ((uint32_t)b[3]) << 24;

  return value;
}

uint16_t SimpleFlashAudio::serialReadUint16LE(Stream &stream, bool &ok) {
  uint8_t b[2];

  if (!serialReadExact(stream, b, 2)) {
    ok = false;
    return 0;
  }

  uint16_t value = 0;
  value |= ((uint16_t)b[0]);
  value |= ((uint16_t)b[1]) << 8;

  return value;
}

void SimpleFlashAudio::serialSendOk(Stream &stream) {
  stream.print("OK\n");
}

void SimpleFlashAudio::serialSendErr(Stream &stream) {
  stream.print("ERR\n");
}

void SimpleFlashAudio::serialPrintInfo(Stream &stream) {
  SFAFlashInfo info = getFlashInfo();

  stream.println("SimpleFlashAudio");
  stream.print("Manufacturer ID: 0x");
  if (info.manufacturerId < 16) stream.print("0");
  stream.println(info.manufacturerId, HEX);

  stream.print("Memory type: 0x");
  if (info.memoryType < 16) stream.print("0");
  stream.println(info.memoryType, HEX);

  stream.print("Capacity ID: 0x");
  if (info.capacityId < 16) stream.print("0");
  stream.println(info.capacityId, HEX);

  stream.print("Flash size: ");
  stream.println(info.sizeBytes);

  stream.print("Used size: ");
  stream.println(getUsedSize());

  stream.print("Free size: ");
  stream.println(getFreeSize());

  stream.print("Used percent: ");
  stream.println(getUsedPercent());

  stream.print("Audio count: ");
  stream.println(getAudioCount());
}

void SimpleFlashAudio::serialPrintList(Stream &stream) {
  uint8_t maxTracks = _legacyMode ? SFA_LEGACY_MAX_TRACKS : SFA_MAX_TRACKS;

  for (uint8_t i = 1; i <= maxTracks; i++) {
    SFAAudioInfo info;

    if (getAudioInfo(i, info)) {
      stream.print(i);
      stream.print(": ");
      stream.print(info.name);
      stream.print(" ");
      stream.print(info.sampleRate);
      stream.print(" Hz ");
      stream.print(info.size);
      stream.println(" bytes");
    }
  }
}
