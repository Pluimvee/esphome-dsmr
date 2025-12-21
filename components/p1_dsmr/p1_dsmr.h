#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <cmath>

namespace esphome {
namespace p1_dsmr {

// Simple DSMR/P1 reader: PollingComponent + UARTDevice that parses telegrams,
// stores OBIS values, and publishes them to configured sensors.
class P1DsmrComponent : public PollingComponent, public uart::UARTDevice {
 public:
  void set_obis_validity(uint32_t ms) { obis_validity_ms_ = ms; }
  void register_obis_sensor(const std::string &obis, sensor::Sensor *sensor);

  void setup() override;
  void loop() override;
  void update() override;

 protected:
  struct ObisValue {
    float value{NAN};
    uint32_t t_ms{0};
  };

  // Config + bookkeeping
  uint32_t obis_validity_ms_{10000};

  std::string rx_buffer_;
  std::unordered_map<std::string, ObisValue> obis_values_;
  std::vector<std::pair<std::string, sensor::Sensor *>> sensors_;

  void read_uart_();
  void parse_telegram_(char *buf);
  void set_value_(const std::string &obis, float value);
  void expire_stale_(uint32_t now_ms);
  void publish_sensors_();
};

}  // namespace p1_dsmr
}  // namespace esphome
