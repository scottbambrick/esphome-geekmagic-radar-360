# ESPHome for the GeekMagic Radar 360

Working [ESPHome](https://esphome.io) support for the **GeekMagic Radar 360**, the 2.1" round
"ADS-B desktop live flight tracker / weather radar" display sold on AliExpress and at
shop.geekmagic.cc. It is model RD03, and the stock firmware's product id is `radar21en`.

GeekMagic doesn't publish the pinout, the panel driver or the firmware, and the case is printed
shut. Everything here was worked out over Wi-Fi from firmware running on the device itself, with
no disassembly and no serial cable. This repo gives you the pinout and a bare-minimum config
that brings the screen up ([`radar-360.yaml`](radar-360.yaml)). What you build on top is up to you.

![status](https://img.shields.io/badge/display-working-brightgreen)

## Hardware (as found)

| | |
|---|---|
| SoC | ESP32-S3R8: dual core, 8 MB octal PSRAM, 16 MB flash |
| Panel | 2.1" round IPS, **360 x 360**, RGB565 |
| Controller | **GC9B72-compatible**; accepts the GC9B72 init and runs at about 58.9 Hz |
| Bus | **4-wire SPI, mode 0**, 20 MHz tested; write-only (no MISO) |
| Input | one push button |
| Other | no touch, no I2C devices, USB-C is power only |

### Pin map

| Function | GPIO | Notes |
|---|---|---|
| SPI CLK | **12** | |
| SPI MOSI | **11** | |
| DC | **13** | |
| RST | **8** | |
| CS | none | tied low on the board |
| TE | **15** | output from the panel (tearing effect, idle low); optional |
| Backlight | **5** | **active LOW**, PWM-dimmable; has an external pull-up, so it is off by default |
| Button | **38** | active low, external pull-up |

GPIO 26-37 belong to the flash/PSRAM. Every other GPIO is unconnected (all of them float).

## Flashing ESPHome (no cable needed)

The stock firmware has a local web UI with an unauthenticated firmware upload. Use that once,
and all later updates go over normal ESPHome OTA.

1. Set up the device with the stock app or web UI so it joins your Wi-Fi, and note its IP.
2. Add `wifi_ssid`, `wifi_password` and `api_key` to your `secrets.yaml`.
3. Compile [`radar-360.yaml`](radar-360.yaml) **with `name: radar21en` unchanged**, then download
   the OTA binary (`firmware.ota.bin`).
4. Upload it to the stock firmware:

   ```sh
   curl -X POST http://<device-ip>/api/ota \
        -H 'Content-Type: application/octet-stream' \
        --data-binary @firmware.ota.bin
   ```

   The stock updater only accepts an image whose ESP-IDF app descriptor `project_name` is
   exactly `radar21en`. Anything else is rejected with `HTTP 400 Expected radar21en firmware`.
   ESPHome writes `substitutions.name` into that field (the `project:` block does not), which is
   why the name must stay `radar21en` for this first upload.

5. The device reboots into ESPHome. Later updates go through ESPHome OTA, which has no name check,
   so you can rename the device then.

> **There is no way back.** GeekMagic doesn't publish the stock firmware. Its cloud updater is
> TLS-pinned, so it can't be intercepted, and the stock image has no factory partition: both OTA
> slots get overwritten. Only flash this if you're happy to keep ESPHome. ESPHome's `safe_mode`
> (rollback after repeated failed boots) is your safety net from then on.

## Gotchas

- **Backlight is active low.** Driving GPIO5 high turns it *off*. Use `inverted: true`.
- **Rotation:** ESPHome's `rotation:` works only if the init sequence does **not** send
  `0x36` (MADCTL). The public GC9B72 init ends with `[0x36, 0x00]`. That line is removed here,
  otherwise rotation silently does nothing.
- **Don't add a CS pin.** CS is hard-wired low. Leave `cs_pin` unset.
- `invert_colors: false` and `color_order: rgb` give correct colours.
- **Full-frame images:** a 360x360 RGB565 frame is about 253 KB, so keep `psram:` enabled when
  drawing large images (for example with `online_image`).

## How the pins were found

This is the short version; [docs/reverse-engineering.md](docs/reverse-engineering.md) has the
details, including the dead ends.

1. The stock `/api/status` reported `te_timeouts`. A TE (tearing effect) signal only exists on
   panels with their own frame memory (SPI, QSPI or 8080), not on RGB-parallel ones. That ruled
   out the RGB-bus guesses.
2. A camera sweep found the backlight: GPIO5, active low. Every earlier "black screen" result
   was void, because the backlight had been off the whole time.
3. With the owner pressing the button, a pin-change logger found GPIO38.
4. An ADC pull-up/pull-down check found that every pin floats except 5, 38 and **15**. GPIO15 is
   actively driven low, which suggests a panel TE output.
5. A group-testing scan tried randomly assigning all 30 free pins to CLK/MOSI/DC/CS-low/RST groups at once. It
   bit-banged a GC9B72 init plus TEON, then watched GPIO15 for TE pulses. It hit on attempt 40
   and decoded the pins by halving the groups on the device.
6. Control experiments proved
   which pin is DC. A data-dependent change in TE timing only happens when DC is right.

## Credits

- GC9B72 init sequence: [MaliosDark/Arduino_GC9B72](https://github.com/MaliosDark/Arduino_GC9B72)
  (ported from xboot `fb-gc9b72.c`).
- Built with ESPHome's `mipi_spi` display platform.

Not affiliated with GeekMagic. MIT licensed.
