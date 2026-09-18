#include "heltec_wireless_paper_v12.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace heltec_wireless_paper_v12 {

static const char *const TAG = "heltec_wireless_paper_v12";

void HeltecWirelessPaperV12::setup() {
  this->dc_pin_->setup();
  this->dc_pin_->digital_write(false);

  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
  }

  if (this->busy_pin_ != nullptr) {
    this->busy_pin_->setup();
  }

  this->spi_setup();
  this->init_internal_(this->get_buffer_length_());
  memset(this->buffer_, 0xFF, this->get_buffer_length_());

  this->init_display_();
}

// SSD1682 initialisation. Panel: HT_E0213A367, 122 source x 250 gate.
void HeltecWirelessPaperV12::init_display_() {
  this->reset_();

  // Software reset
  this->command_(0x12);
  delay(10);
  this->wait_busy_(2000, "sw reset");

  // Driver output control: scan gates 0..249. Two parameters, not three.
  this->command_(0x01);
  this->data_(0xF9);
  this->data_(0x00);

  // Data entry mode: X increment, Y increment
  this->command_(0x11);
  this->data_(0x03);

  // RAM X window: byte 0 .. 15  (16 bytes = 128 pixels)
  this->command_(0x44);
  this->data_(0x00);
  this->data_(0x0F);

  // RAM Y window: row 0 .. 249. The SSD1682 takes single-byte Y values here,
  // unlike the SSD1680 family which expects two bytes per value.
  this->command_(0x45);
  this->data_(0x00);
  this->data_(0xF9);

  // Use the internal temperature sensor for waveform selection
  this->command_(0x18);
  this->data_(0x80);

  this->set_ram_pointer_();
}

// Selects which waveform the controller uses. Command 0x37 is undocumented;
// the values are Heltec's. Without it no display mode is selected and the
// refresh moves no pixels.
void HeltecWirelessPaperV12::config_waveform_(bool fast) {
  this->command_(0x37);
  this->data_(0x40);
  this->data_(0x80);
  this->data_(0x03);
  this->data_(0x0E);

  // Border waveform
  this->command_(0x3C);
  this->data_(fast ? this->border_fast_ : this->border_full_);
}

void HeltecWirelessPaperV12::set_ram_pointer_() {
  // RAM X address counter
  this->command_(0x4E);
  this->data_(0x00);

  // RAM Y address counter - single byte on the SSD1682
  this->command_(0x4F);
  this->data_(0x00);
}

void HeltecWirelessPaperV12::write_ram_(uint8_t command) {
  this->set_ram_pointer_();
  this->command_(command);
  this->data_array_(this->buffer_, this->get_buffer_length_());
  App.feed_wdt();
}

void HeltecWirelessPaperV12::update() {
  this->do_update_();
  this->send_buffer_();
}

void HeltecWirelessPaperV12::send_buffer_() {
  bool full = (this->update_count_ % this->full_update_every_) == 0;
  this->update_count_++;

  ESP_LOGD(TAG, "%s refresh #%lu", full ? "Full" : "Fast", (unsigned long) this->update_count_);

  // A hardware reset clears both RAM banks, which would break the differential
  // refresh, so only re-initialise on a full update.
  if (full) {
    this->init_display_();
  }

  this->config_waveform_(!full);

  // New image into the B/W RAM. 1 = white, 0 = black.
  this->write_ram_(0x24);

  if (full) {
    // Keep the "old" RAM in sync so the next fast refresh has a valid base.
    this->write_ram_(0x26);
  }

  // Update sequence: 0xF7 loads the LUT from OTP and runs display mode 1
  // (full refresh), 0xFF runs display mode 2 (differential refresh).
  this->command_(0x22);
  this->data_(full ? 0xF7 : 0xFF);

  // Master activation
  this->command_(0x20);
  this->wait_busy_(full ? 30000 : 10000, full ? "full refresh" : "fast refresh");

  if (!full) {
    // After a differential refresh the panel now shows the new image, so it
    // becomes the base for the next one.
    this->write_ram_(0x26);
  }
}

void HeltecWirelessPaperV12::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x < 0 || x >= VISIBLE_WIDTH || y < 0 || y >= NATIVE_HEIGHT)
    return;

  // Buffer uses NATIVE_WIDTH (128) for row stride, not VISIBLE_WIDTH (122)
  uint32_t pos = x / 8u + y * (NATIVE_WIDTH / 8u);

  if (color.is_on()) {
    this->buffer_[pos] &= ~(0x80 >> (x & 7));
  } else {
    this->buffer_[pos] |= (0x80 >> (x & 7));
  }
}

void HeltecWirelessPaperV12::fill(Color color) {
  uint8_t fill = color.is_on() ? 0x00 : 0xFF;
  memset(this->buffer_, fill, this->get_buffer_length_());
}

void HeltecWirelessPaperV12::command_(uint8_t cmd) {
  this->dc_pin_->digital_write(false);
  this->enable();
  this->write_byte(cmd);
  this->disable();
}

void HeltecWirelessPaperV12::data_(uint8_t val) {
  this->dc_pin_->digital_write(true);
  this->enable();
  this->write_byte(val);
  this->disable();
}

// Chunked bulk transfer. Byte-wise writing works too but takes seconds for
// 4000 bytes; 1 kB blocks stay well inside the ESP-IDF transaction limits.
// If the image ever comes out scrambled, drop CHUNK to 1 to go byte-wise.
void HeltecWirelessPaperV12::data_array_(const uint8_t *data, size_t len) {
  static const size_t CHUNK = 1024;
  this->dc_pin_->digital_write(true);
  for (size_t offset = 0; offset < len; offset += CHUNK) {
    size_t n = (len - offset) < CHUNK ? (len - offset) : CHUNK;
    this->enable();
    this->write_array(data + offset, n);
    this->disable();
    App.feed_wdt();
  }
}

// BUSY is HIGH while the controller is working (Solomon Systech logic).
// Short operations may already be finished when we get here - normal, not an
// error, so it is only logged at debug level.
void HeltecWirelessPaperV12::wait_busy_(uint32_t timeout_ms, const char *what) {
  if (this->busy_pin_ == nullptr) {
    delay(2000);
    return;
  }

  uint32_t start = millis();
  bool went_high = false;
  while (millis() - start < 100) {
    if (this->busy_pin_->digital_read()) {
      went_high = true;
      break;
    }
    delay(1);
  }

  if (!went_high) {
    ESP_LOGD(TAG, "wait_busy [%s]: no busy pulse seen", what);
    return;
  }

  start = millis();
  while (this->busy_pin_->digital_read()) {
    if (millis() - start > timeout_ms) {
      ESP_LOGW(TAG, "wait_busy [%s]: timeout after %lums", what, (unsigned long) timeout_ms);
      return;
    }
    App.feed_wdt();
    delay(10);
  }
  ESP_LOGD(TAG, "wait_busy [%s]: busy for %lums", what, (unsigned long) (millis() - start));
}

// RESET is active low.
void HeltecWirelessPaperV12::reset_() {
  if (this->reset_pin_ == nullptr) {
    delay(20);
    return;
  }

  this->reset_pin_->digital_write(true);
  delay(20);
  this->reset_pin_->digital_write(false);
  delay(10);
  this->reset_pin_->digital_write(true);
  delay(20);
  this->wait_busy_(2000, "hw reset");
}

void HeltecWirelessPaperV12::dump_config() {
  LOG_DISPLAY("", "Heltec Wireless Paper V1.2 (SSD1682 / E0213A367)", this);
  ESP_LOGCONFIG(TAG, "  Visible: %dx%d, RAM: %dx%d, Buffer: %lu bytes", VISIBLE_WIDTH, NATIVE_HEIGHT, NATIVE_WIDTH,
                NATIVE_HEIGHT, (unsigned long) this->get_buffer_length_());
  ESP_LOGCONFIG(TAG, "  Full update every: %lu", (unsigned long) this->full_update_every_);
  ESP_LOGCONFIG(TAG, "  Border: full 0x%02X, fast 0x%02X", this->border_full_, this->border_fast_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
}

}  // namespace heltec_wireless_paper
}  // namespace esphome
