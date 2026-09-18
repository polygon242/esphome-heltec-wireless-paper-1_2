# ESPHome Heltec Wireless Paper Display Component

Custom ESPHome display component for the **Heltec Wireless Paper** board, modified to run with the hardware V1.2.
It is based on the Repro of https://github.com/RonnyHempel1981 and modified with the info from https://github.com/CyberBasti .


The Heltec Wireless Paper uses a 2.13" e-paper display with a **V1.2 HT_E0213A367 — SSD1682, 122 source x 250 gate**  

## Hardware

| Feature | Details |
|---------|---------|
| MCU | ESP32-S3 |
| Display | 2.13" e-paper (V1.2 = HT_E0213A367) |
| Controller | V1.2 = SSD1682 |
| Resolution | 250 x 122 (landscape) / 122 x 250 (portrait) |
| Colors | Black / White |
| Interface | SPI |

### Pin Assignment

| Function | GPIO |
|----------|------|
| SPI CLK | GPIO3 |
| SPI MOSI | GPIO2 |
| CS | GPIO4 |
| DC | GPIO5 |
| Reset | GPIO6 |
| Busy | GPIO7 |
| Vext Power | GPIO45 (active LOW) |

> **Important:** GPIO45 (Vext) must be driven LOW to power the display. It is a strapping pin on the ESP32-S3 and requires `ignore_strapping_warning: true`.

## Installation

Add to your ESPHome YAML:

```yaml
external_components:
  - source: github://polygon242/esphome-heltec-wireless-paper-1_2@Fix-for-V1.2
    components: [heltec_wireless_paper_v12]
    refresh: 0s
```

## Full Example

```yaml
esphome:
  name: my-wireless-paper
  friendly_name: My Wireless Paper
  platformio_options:
    board_build.flash_mode: dio
  on_boot:
    priority: 600.0
    then:
      - output.turn_on: vext_power
      - delay: 200ms

esp32:
  board: esp32-s3-devkitc-1
  variant: esp32s3
  flash_size: 8MB
  framework:
    type: esp-idf

logger:
  hardware_uart: UART0

api:

ota:
  - platform: esphome

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

external_components:
  - source:
      type: git
      url: https://github.com/RonnyHempel1981/esphome-heltec-wireless-paper
      ref: main
    components: [heltec_wireless_paper]

# Vext power must be enabled before display init
output:
  - platform: gpio
    pin:
      number: GPIO45
      inverted: true
      ignore_strapping_warning: true
    id: vext_power

spi:
  clk_pin: GPIO3
  mosi_pin: GPIO2

font:
  - file: "gfonts://Inter"
    id: font_main
    size: 16
  - file: "gfonts://Inter"
    id: font_small
    size: 10

display:
  - platform: heltec_wireless_paper
    id: epaper
    cs_pin: GPIO4
    dc_pin: GPIO5
    reset_pin: GPIO6
    busy_pin: GPIO7
    rotation: 270
    full_update_every: 30
    update_interval: 60s
    lambda: |-
      it.print(0, 0, id(font_main), "Hello Wireless Paper!");
      it.printf(0, 24, id(font_small), "WiFi: %s", wifi::global_wifi_component->is_connected() ? "OK" : "NO");
```

## Configuration Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `cs_pin` | pin | **required** | SPI chip select |
| `dc_pin` | pin | **required** | Data/Command pin |
| `reset_pin` | pin | optional | Hardware reset pin |
| `busy_pin` | pin | optional | Busy status pin |
| `rotation` | int | `0` | Display rotation: 0, 90, 180, 270 |
| `full_update_every` | int | `30` | Full refresh every N updates (reduces ghosting) |
| `update_interval` | time | `60s` | How often to redraw |
| `lambda` | lambda | optional | Drawing code using ESPHome display API |

### Rotation

- `0` - Portrait 122x250
- `90` - Landscape 250x122 (USB port on left)
- `270` - Landscape 250x122 (USB port on right, recommended)

### New in V1.2  
new border_full / border_fast options, exposing the 0x3C border waveform to
YAML. Heltec's 0x01 / 0x81 leave the border white, 0x00 / 0x80 make it
black. Defaults keep the existing behaviour.

## Notes

- `board_build.flash_mode: dio` is required to prevent boot loops on the ESP32-S3.  -> not needed?
- `hardware_uart: UART0` is required for serial logging.
- `full_update_every` controls how often a full e-paper refresh cycle is performed. Lower values reduce ghosting but cause more flicker.
- All standard ESPHome [display drawing functions](https://esphome.io/components/display/) are supported: `print`, `printf`, `line`, `rectangle`, `circle`, `image`, etc.

## Why not waveshare_epaper?

The standard `waveshare_epaper` component sends SSD1680 commands (0x12 SW_RESET, 0x01 DRV_OUT_CTL, etc.). The Heltec Wireless Paper's JD79656 controller requires UC8151D commands (0x06 BOOSTER_SOFT_START, 0x04 POWER_ON, 0x00 PANEL_SETTING). Using any `waveshare_epaper` model results in a display that refreshes but shows no content.

## License

MIT
