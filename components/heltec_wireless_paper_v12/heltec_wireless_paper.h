#pragma once

#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/display/display_buffer.h"

namespace esphome {
namespace heltec_wireless_paper {

/// ESPHome display driver for the Heltec Wireless Paper V1.2 (SSD1682 controller).
/// Visible resolution: 122 x 250 (portrait). Use rotation: 90 or 270 for 250x122 landscape.
/// The controller addresses 128x250 but only 122 columns are physically visible.
///
/// Board revisions differ in their panel:
///   V1.0  DEPG0213BNS800
///   V1.1  LCMEN2R13EFC1  - JD79656 / UC8151D, BUSY active LOW
///   V1.2  HT_E0213A367   - SSD1682, BUSY active HIGH   <-- this driver
/// V1.2 boards are silkscreened "1.1.1" on the PCB.
///
/// Command 0x37 and its parameters are undocumented; the values come from
/// Heltec via the Meshtastic NicheGraphics driver for this panel. Without
/// them the controller runs the refresh but selects no display mode, so
/// nothing visible happens.
///
/// GPIO45 (Vext, active LOW) must be enabled before this component initializes
/// to power the display. Configure it as a gpio switch with inverted: true and
/// restore_mode: ALWAYS_ON.
class HeltecWirelessPaper : public display::DisplayBuffer,
                            public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                                  spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_4MHZ> {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  void set_dc_pin(GPIOPin *pin) { dc_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { reset_pin_ = pin; }
  void set_busy_pin(GPIOPin *pin) { busy_pin_ = pin; }
  void set_full_update_every(uint32_t val) { full_update_every_ = val; }

  // Border waveform (command 0x3C). Heltec's values are 0x01 for a full and
  // 0x81 for a fast refresh, which leave the border white. Try 0x00, 0x02,
  // 0x40, 0x50 or 0x60 for a dark border - the bit meanings are undocumented
  // for this controller, so it is a matter of trying.
  void set_border_full(uint8_t val) { border_full_ = val; }
  void set_border_fast(uint8_t val) { border_fast_ = val; }

  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_BINARY; }

 protected:
  static const int NATIVE_WIDTH = 128;   // controller RAM width
  static const int VISIBLE_WIDTH = 122;  // physically visible columns
  static const int NATIVE_HEIGHT = 250;

  int get_width_internal() override { return VISIBLE_WIDTH; }
  int get_height_internal() override { return NATIVE_HEIGHT; }
  uint32_t get_buffer_length_() { return NATIVE_WIDTH / 8u * NATIVE_HEIGHT; }  // 4000 bytes

  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  void fill(Color color) override;

  void init_display_();
  void send_buffer_();
  void config_waveform_(bool fast);
  void write_ram_(uint8_t command);
  void set_ram_pointer_();
  void command_(uint8_t cmd);
  void data_(uint8_t val);
  void data_array_(const uint8_t *data, size_t len);
  void wait_busy_(uint32_t timeout_ms, const char *what);
  void reset_();

  GPIOPin *dc_pin_{nullptr};
  GPIOPin *reset_pin_{nullptr};
  GPIOPin *busy_pin_{nullptr};

  uint32_t full_update_every_{15};
  uint32_t update_count_{0};
  uint8_t border_full_{0x01};
  uint8_t border_fast_{0x81};
};

}  // namespace heltec_wireless_paper
}  // namespace esphome
