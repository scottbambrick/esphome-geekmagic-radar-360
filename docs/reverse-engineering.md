# Reverse-engineering the Radar 360 display

How the pin map was found on a sealed device: no UART, no case opening, no vendor firmware.
Everything was done by flashing small ESPHome test firmwares over Wi-Fi and watching the
results, through Home Assistant entities, device logs and a cheap ESP32-CAM pointed at the
screen. These notes are for anyone working on a similar undocumented display.

## Starting facts

- **Stock web UI:** `http://radar.local` offers a "Local Firmware Update", which is a raw
  `POST /api/ota` with no authentication. It checks only that `project_name` is `radar21en`.
- **Stock `/api/status`:**
  - It exposed `heap_psram` (multi-MB), `renderer: canvas` (LVGL), `display_fps` of about 1.4-1.9
    and `te_timeouts`.
  - The low fps suggested plain single-lane SPI.
  - `te_timeouts` proved there is a TE line. TE only exists on panels with GRAM, which also
    explains why the last stock image stayed on screen after reflashing.
- **Product listing:** 360x360, ESP32-S3.
- **Cloud OTA:** `ota.geekmagic.cc` validates certificates, and every guessed firmware path
  returned 404. The stock image was never obtained.
- **After the first ESPHome flash:** the partition table is `nvs, otadata, phy_init, ota_0,
  ota_1 (5 MB each), storage (6 MB LittleFS)`. There is no factory app, so the stock firmware
  was gone.

## Dead ends (so you don't repeat them)

All of these showed nothing. Most were also **invalid tests**, because the backlight was off the
whole time (see below).

- **RGB-parallel (ST7701S, 480x480):** presets for MAKERFABS-4, UEDX48480021, ZX2D10GE01R,
  Elecrow CrowPanel 2.1, Waveshare 2.1, Guition 4848S040, Makerfabs MaTouch 2.1 and an LCD Wizard
  map, plus bit-banged 3-wire 9-bit ST7701 init scans over every clk/mosi pair.
- **QSPI (ST77916 360x360):** pin sets from Waveshare 1.85/2.16, JC3636W518 (+V2) and ESP-VOCAT,
  with several reset variants.
- **I2C:** a bit-banged scan of every pin pair at addresses 0x08-0x77 found no devices at all
  (no touch, RTC, IMU or IO expander).
- **TE scans with pull-downs on idle pins:** these held the (then unknown) RST pin low, so the
  panel could never wake up. This was the key mistake.
- **GC9B72 on guessed maps:** example wirings from the GC9B72 README and another GeekMagic
  product (`GeekMagic-S3`) didn't match.

## What worked

### 1. Backlight first

An ESP32-CAM watched the screen. Its JPEGs were reduced to a tiny luminance thumbnail by
decoding only the DC coefficients, which is enough to judge "lit or not" without image
libraries. Toggling candidate pins found it: **GPIO5 driven LOW** gives a faint glow, and
repeated 45-second alternation confirmed it. GPIO5 has an external pull-up, so the backlight is
off by default. A later PWM fade test confirmed it is dimmable.

### 2. Button

The owner pressed the button while a small test firmware logged level changes on every
pin. Only GPIO38 changed. That ruled out GPIO38 as CS, which an earlier scan had assumed.

### 3. Electrical characterisation (Stage 0)

A test firmware reads every
free pin's voltage with the internal pull-up, then with the pull-down.

- Every pin floats, except 5 (pull-up), 38 (pull-up) and **15**.
- GPIO15 reads 3 mV with the pull-up on, so something is actively driving it low: a panel TE
  output at idle.
- There are no pull resistors on RST or CS, so they must be GPIO-driven or tied off on the
  board.
- This stage frees GPIO19/20 (USB D-/D+) and 43/44 (UART) for testing. It sets
  `logger: baud_rate: 0` and clears the USB-Serial-JTAG pad-enable bit.

### 4. Group-testing scan (Stage 1)

Trying every (clk, mosi, dc, cs, rst) combination one at a time is about 30^5 tests, which is
far too many. Instead, each attempt:

- **Assigns** all 30 candidate pins at random to five groups: CLK, MOSI, DC, CS-held-low and
  RST-pulsed. Pins in the same group are driven together through the GPIO W1TS/W1TC registers,
  at the lowest drive strength, so several pins act as one.
- **Wakes the panel:** pulses RST, waits 120 ms, bit-bangs the GC9B72 init, then sends SLPOUT
  and waits 120 ms, then sends TEON and DISPON.
- **Listens** for 60 ms and counts edges on GPIO15.

A correct assignment happens with probability 1/5^5 = 1/3125 per attempt, so 25,000 attempts give
more than 99.9% coverage in a couple of hours, unattended. It hit at **attempt 40**. The firmware
then confirmed the hit twice and narrowed each group by halving it, all on the device.

### 5. Proving DC vs CS

The decode reported GPIO13 as "CS". A second test firmware
ran controls that measure TE timing:

- reset only, SLPOUT only, and SLPOUT+TEON all produce TE (the panel free-runs);
- the full GC9B72 init changes the TE high time from **699 to 212 us**;
- TEON with data byte `0x01` (V+H blanking mode) gives **line-rate TE**, about 8,400 rises in
  200 ms.

Those data-dependent changes only happen if GPIO13 is DC, so CS is tied low on the board.
Oddly, SWRESET (0x01) didn't stop TE.

### 6. Pixels

ESPHome `mipi_spi` with `model: custom` and the GC9B72 init sequence at 20 MHz showed an image
on the first try. The one fix needed afterwards was removing `[0x36, 0x00]` from the init, so
that `rotation:` works.

## Tips for other panels

- Find and turn on the backlight before trusting any "nothing on screen" result.
- A TE pin is a great camera-free signal that the panel has been initialised. Look for a pin
  that is driven (it ignores the internal pull-up) and pulses at about 60 Hz once TEON is sent.
- Never hold unknown pins with pull-downs while scanning: one of them may be RST.
- Group testing turns a brute-force search that would take years into one that takes hours.
