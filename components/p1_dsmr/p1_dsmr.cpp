#include "p1_dsmr.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <strings.h>
#include <sys/types.h>
#include <cmath>

namespace esphome {
namespace p1_dsmr {

static const char *TAG = "p1_dsmr";

////////////////////////////////////////////////////////////////////////////////////////////////////////
void P1DsmrComponent::setup() {
  rx_buffer_.reserve(2048);
  ESP_LOGI(TAG, "DSMR reader ready; obis-validity=%u ms", (unsigned) obis_validity_ms_);
}

void P1DsmrComponent::loop() {
  // Process incoming bytes continuously; publishing happens in update().
  this->read_uart_();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void P1DsmrComponent::update() {
  const uint32_t now_ms = millis();
  this->expire_stale_(now_ms);
  this->publish_sensors_();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void P1DsmrComponent::register_obis_sensor(const std::string &obis, sensor::Sensor *sensor) {
  if (obis.empty() || sensor == nullptr)
    return;
  sensors_.push_back({obis, sensor});
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Some helpers
////////////////////////////////////////////////////////////////////////////////////////////////////////
static inline bool is_obis_char(char c) {
  return (c >= '0' && c <= '9') || c == '-' || c == ':' || c == '.';
}

static inline uint8_t hex_nibble_(unsigned char c) {
  if (c <= '9')
    return static_cast<uint8_t>(c - '0');
  c = static_cast<unsigned char>(c & ~0x20);
  return static_cast<uint8_t>(c - 'A' + 10);
}

static inline uint16_t crc16_dsmr_(const uint8_t *data, size_t len) {
  uint16_t crc = 0x0000;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int j = 0; j < 8; j++)
      crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : (crc >> 1);
  }
  return crc;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Reading the uart and validating telegrams, when valid the telegram will be parsed
////////////////////////////////////////////////////////////////////////////////////////////////////////
void P1DsmrComponent::read_uart_() 
{
  while (this->available()) {
    uint8_t b;
    if (!this->read_byte(&b))
      break;
    rx_buffer_.push_back(static_cast<char>(b));

    const size_t n = rx_buffer_.size();
    if (n > 4096) {
      ESP_LOGW(TAG, "UART buffer overrun, flushing %u bytes", (unsigned) n);
      rx_buffer_.clear();
      continue;
    }
    // find the end of the telegram: '!' + 4 hex
    if (n >= 5 && rx_buffer_[n - 5] == '!' && std::isxdigit(static_cast<unsigned char>(rx_buffer_[n - 4])) &&
        std::isxdigit(static_cast<unsigned char>(rx_buffer_[n - 3])) &&
        std::isxdigit(static_cast<unsigned char>(rx_buffer_[n - 2])) &&
        std::isxdigit(static_cast<unsigned char>(rx_buffer_[n - 1]))) {
      ssize_t start = -1;
      ssize_t end = static_cast<ssize_t>(n) - 5;  // set the end to just before the '!'
      // from the end we backward to fnd the start of the telegram '/'
      for (ssize_t i = end; i >= 0; --i) {
        if (rx_buffer_[i] == '/') {
          start = i;
          break;
        }
      }
      if (start >= 0) {
        // start has also been found, lets check the CRC16 checksum
        const uint16_t rx_crc = static_cast<uint16_t>((hex_nibble_(rx_buffer_[n - 4]) << 12) |
                                                      (hex_nibble_(rx_buffer_[n - 3]) << 8) |
                                                      (hex_nibble_(rx_buffer_[n - 2]) << 4) |
                                                       hex_nibble_(rx_buffer_[n - 1]));
        const uint8_t *crc_start = reinterpret_cast<const uint8_t *>(rx_buffer_.data() + start);
        const size_t crc_len = static_cast<size_t>(end - start + 1);
        const uint16_t calc_crc = crc16_dsmr_(crc_start, crc_len);
        if (calc_crc == rx_crc) {
          char *telegram = rx_buffer_.data() + start;
          rx_buffer_[end] = '\0';
          parse_telegram_(telegram);
        } else {
          ESP_LOGW(TAG, "DSMR checksum mismatch: calc=%04X recv=%04X", calc_crc, rx_crc);
        }
      } else {
        ESP_LOGW(TAG, "Unable to find start of DSMR telegram, clearing buffer");
      }
      rx_buffer_.clear();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// OBIS format "1-0:1.7.0(00.123*kW)" *<unit> is optional
////////////////////////////////////////////////////////////////////////////////////////////////////////
static int _log_line_start = 0;
static constexpr int kLogLinesPerParse = 10;

void P1DsmrComponent::parse_telegram_(char *buf) {
  int lines = 0;
  int records = 0;
  char *p = buf;

  const int log_start = _log_line_start;
  const int log_end = log_start + kLogLinesPerParse - 1;
  while (*p) {
    // determine the first line in the telegram
    char *key = p;
    // find the line end and move p to the next line
    char *end = std::strchr(p, '\n');
    if (end) {
      *end = '\0';  // truncate the line
      p = end + 1;  // move p to the next line
    } else {
      p += std::strlen(p);
      end = p;
    }
    lines++;
    if (lines >= log_start && lines <= log_end)
      ESP_LOGD(TAG, "DSMR telegram line %d: %s", lines, key);

    // skip lines which indicate the start and end of the telegram
    if (key[0] == '/' || key[0] == '!')
      continue;

      // cleanify the obis key by skipping leading and trailing garbage
    while (*key && !is_obis_char(*key))
      key++;
    char *key_end = key;
    while (*key_end && is_obis_char(*key_end))
      key_end++;

    // key_end most likely points to the '(', so we do NOT YET nul-terminate the obis key
    char *value = std::strrchr(key_end, '('); // we only support the last value in case of multiple parentheses
    if (!value)
      continue;
    value++;          // move to the start of the value
    *key_end = '\0';  // now is safe to terminate the obis key !!!
    char *value_end = std::strchr(value, ')');
    if (!value_end)
      continue;
    *value_end = '\0';

    // parse the value to a float
    char *endp = nullptr;
    double dv = std::strtod(value, &endp);  // only first parenthesis parsed to numeric
    if (endp == value) {
      continue;
    }
    if (endp && *endp == '*') {
      const char *unit = endp + 1;
      if (strncasecmp(unit, "kWh", 3) == 0) {
        // already in kWh
      } else if (strncasecmp(unit, "MWh", 3) == 0) {
        dv *= 1000.0;     // energie in kWh
      } else if (strncasecmp(unit, "Wh", 2) == 0) {
        dv /= 1000.0;     // energie in kWh
      } else if (strncasecmp(unit, "kW", 2) == 0) {
        dv *= 1000.0;     // power in Watts
      } else if (strncasecmp(unit, "MW", 2) == 0) {
        dv *= 1000000.0;  // power in Watts
      }
      // Alternative parsing supporting more units
    }
    set_value_(key, static_cast<float>(dv));  // keep numeric for existing sensors
    records++;
  }
  if (lines > 0) {
    _log_line_start += kLogLinesPerParse;
    if (_log_line_start > lines)
      _log_line_start = 0;
  }
  if (records > 0) {
    ESP_LOGI(TAG, "Parsed DSMR telegram: %d lines, %d records", lines, records);
  } else {
    ESP_LOGW(TAG, "Telegram contained no usable records");
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void P1DsmrComponent::set_value_(const std::string &obis, float value) {
  auto &entry = obis_values_[obis];
  entry.value = value;
  entry.t_ms = millis();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void P1DsmrComponent::expire_stale_(uint32_t now_ms) {
  if (obis_validity_ms_ == 0)
    return;
  for (auto &kv : obis_values_) {
    if (kv.second.t_ms != 0 && (now_ms - kv.second.t_ms) > obis_validity_ms_)
      kv.second.value = NAN;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void P1DsmrComponent::publish_sensors_() {
  for (const auto &pair : sensors_) {
    const auto &obis = pair.first;
    sensor::Sensor *s = pair.second;
    if (auto it = obis_values_.find(obis); it != obis_values_.end()) {
      s->publish_state(it->second.value);  // NaN from stale entries -> unavailable in HA
    } else {
      s->publish_state(NAN);               // Explicitly mark as unavailable if never seen
    }
  }
}

}  // namespace p1_dsmr
}  // namespace esphome
