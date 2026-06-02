# Flash format

## Modern SimpleFlashAudio format

```text
0x000000  header
0x000040  audio table
0x001000  audio data
```

## Header

```text
magic: SFA1
version
max tracks
data start address
flash size
used bytes
audio count
```

## Entry

Each entry is 64 bytes:

```text
valid flag
index
sample rate
address
size
CRC32
name
```

## Legacy support

The library can also read the older `NSFX` format used by the first uploader.
