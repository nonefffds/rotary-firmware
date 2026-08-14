# Rotary firmware (D1 Mini + BMI160)

Firmware for the **Rotary** monitor-rotation accessory: a Wemos D1 Mini
(ESP8266) with a Bosch BMI160 IMU. It measures gravity's direction in the
monitor plane and streams it over USB serial to the companion Windows app
([rotary](https://github.com/nonefffds/rotary)).

## Wiring

| D1 Mini | BMI160 |
|---------|--------|
| D1 (GPIO5) | SCL |
| D2 (GPIO4) | SDA |
| 3V3 | VCC |
| G  | GND |

## Flashing

PlatformIO (recommended):

```
pio run -t upload
```

or Arduino IDE: install the ESP8266 board package, select *LOLIN(WEMOS) D1
mini*, Upload Speed 115200, upload `src/rotary_bmi160.ino`.

The sketch talks to the BMI160 over raw I2C registers (no libraries needed). If
`ERR BMI160 not found` appears, check wiring/address (auto-detects `0x68`/`0x69`).

## USB driver

The D1 Mini clone uses a CH340 USB-serial chip. If Windows shows it as an
"unknown device" instead of a COM port, install the CH340/CH341 driver:
<https://www.wch.cn/downloads/CH341SER_EXE.html>

## License

This project is MIT licensed (see LICENSE). It is built with the ESP8266
Arduino core, which is licensed under the GNU LGPL v2.1
(<https://github.com/esp8266/Arduino/blob/master/LICENSE.rst>); see the core's
license for its terms.

## Serial protocol (115200 baud)

- `A=<deg> <ax> <ay> <az>` — live angle (~6×/s) where 0° = upright, +90° =
  right edge down (front view). The host uses this + its calibration offset to
  decide the rotation.
- `ROT=<0|1|2|3>` — sector state (upright / +90° / 180° / −90°) sent on change
  and every 2 s. The host currently decides rotation from the angle, so this is
  informational.

The built-in LED blinks once on each orientation change.
