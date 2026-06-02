# Audio format

Recommended:

```text
Unsigned 8-bit PCM
Mono
RAW stream
8000 / 11025 / 16000 Hz
```

The microcontroller does not decode MP3/WAV/OGG/FLAC directly during playback.

Convert audio on the PC first, then upload RAW bytes to SPI Flash.
