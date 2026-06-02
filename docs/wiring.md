# Wiring

## Arduino Nano + W25Q + PAM8403

```text
Arduino Nano D9  -> RC filter -> PAM8403 input
Arduino Nano D10 -> W25Q CS
Arduino Nano D11 -> W25Q DI / MOSI
Arduino Nano D12 -> W25Q DO / MISO
Arduino Nano D13 -> W25Q CLK
3.3V             -> W25Q VCC
GND              -> common GND
```

W25Q pins:

```text
        _________
 CS   1|         |8  VCC 3.3V
 DO   2|         |7  HOLD -> 3.3V
 WP   3|         |6  CLK
 GND  4|_________|5  DI
```

WP and HOLD should be pulled to 3.3V.

For 5V Arduino boards, use level shifting or resistor dividers for:

```text
CS
DI / MOSI
CLK
```
