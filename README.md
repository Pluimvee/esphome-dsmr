ESPHome DSMR/P1 Reader
======================

Minimal DSMR/P1 reader that parses OBIS codes from a smart meter telegram and publishes them as ESPHome sensors.

Usage
-----

Requires ESPHome v2022.3.0 or newer.

```yaml
logger:
  level: DEBUG  
  baud_rate: 0  # Disable the use of the serial as we need it !!!!!

external_components:
  - source: github://Pluimvee/esphome-dsmr

uart:
  - id: uart_p1
    rx_pin:
      number: GPIO3
      inverted: true
    baud_rate: 115200
    rx_buffer_size: 3000
    # debug:

p1_dsmr:
  uart_id: uart_p1
  update_interval: 1s
  obis_validity: 10s
  sensors:
    - obis: "1-0:1.7.0"
      name: "Power Import"
      unit_of_measurement: "W"
      device_class: power
      state_class: measurement
    - obis: "1-0:2.7.0"
      name: "Power Export"
      unit_of_measurement: "W"
      device_class: power
      state_class: measurement
    - obis: "1-0:1.8.1"
      name: "Energy Import T1"
      unit_of_measurement: "kWh"
      device_class: energy
      state_class: total_increasing
```

Configuration
-------------

- `uart_id` (required): UART bus used for the P1 interface.
- `update_interval` (optional): Polling interval (default `500ms`).
- `obis_validity` (optional): expiration period to define how long OBIS values remain valid without being updated (default `10s`). Set to `0s` to disable expiration.
- `sensors` (optional): List of OBIS sensors. Each entry requires `obis` and supports standard ESPHome sensor options.

Notes
-----

- The component parses the first value in each OBIS line and converts units like `kW`, `MW`, `Wh`, `MWh`.
- If a value is not updated within `obis_validity`, it is published as `NaN` (unavailable in Home Assistant).
- Entries with multiple values (like gas usage) are not yet supported ()

In DEBUG level the DSMR telegram is logged using multiple cycles to prevent log overruns
-----

[15:47:24.077][D][p1_dsmr:147]: DSMR telegram line 1: /ISk5\2MT382-1000
[15:47:24.127][D][p1_dsmr:147]: DSMR telegram line 2:
[15:47:24.127][D][p1_dsmr:147]: DSMR telegram line 3: 1-3:0.2.8(50)
[15:47:24.127][D][p1_dsmr:147]: DSMR telegram line 4: 0-0:1.0.0(101209113020W)
[15:47:24.127][D][p1_dsmr:147]: DSMR telegram line 5: 0-0:96.1.1(4B384547303034303436333935353037)
[15:47:24.127][D][p1_dsmr:147]: DSMR telegram line 6: 1-0:1.8.1(020904.209*kWh)
[15:47:24.127][D][p1_dsmr:147]: DSMR telegram line 7: 1-0:1.8.2(021414.203*kWh)
[15:47:24.127][D][p1_dsmr:147]: DSMR telegram line 8: 1-0:2.8.1(007839.329*kWh)
[15:47:24.127][D][p1_dsmr:147]: DSMR telegram line 9: 1-0:2.8.2(016436.029*kWh)

[15:47:25.088][D][p1_dsmr:147]: DSMR telegram line 10: 0-0:96.14.0(0001)
[15:47:25.150][D][p1_dsmr:147]: DSMR telegram line 11: 1-0:1.7.0(03.085*kW)
[15:47:25.150][D][p1_dsmr:147]: DSMR telegram line 12: 1-0:2.7.0(00.000*kW)
[15:47:25.150][D][p1_dsmr:147]: DSMR telegram line 13: 0-0:96.7.21(00026)
[15:47:25.150][D][p1_dsmr:147]: DSMR telegram line 14: 0-0:96.7.9(00003)
[15:47:25.150][D][p1_dsmr:147]: DSMR telegram line 15: 1-0:99.97.0(2)(0-0:96.7.19)(101208152415W)(0000000240*s)(101208151004W)(0000000301*s)
[15:47:25.150][D][p1_dsmr:147]: DSMR telegram line 16: 1-0:32.32.0(00009)
[15:47:25.150][D][p1_dsmr:147]: DSMR telegram line 17: 1-0:52.32.0(00009)
[15:47:25.150][D][p1_dsmr:147]: DSMR telegram line 18: 1-0:72.32.0(00009)
[15:47:25.150][D][p1_dsmr:147]: DSMR telegram line 19: 1-0:32.36.0(00000)

[15:47:26.080][D][p1_dsmr:147]: DSMR telegram line 20: 1-0:52.36.0(00000)
[15:47:26.128][D][p1_dsmr:147]: DSMR telegram line 21: 1-0:72.36.0(00000)
[15:47:26.128][D][p1_dsmr:147]: DSMR telegram line 22: 0-0:96.13.0()
[15:47:26.128][D][p1_dsmr:147]: DSMR telegram line 23: 1-0:32.7.0(237.0*V)
[15:47:26.128][D][p1_dsmr:147]: DSMR telegram line 24: 1-0:52.7.0(238.0*V)
[15:47:26.128][D][p1_dsmr:147]: DSMR telegram line 25: 1-0:72.7.0(238.0*V)
[15:47:26.128][D][p1_dsmr:147]: DSMR telegram line 26: 1-0:31.7.0(011*A)
[15:47:26.128][D][p1_dsmr:147]: DSMR telegram line 27: 1-0:51.7.0(001*A)
[15:47:26.128][D][p1_dsmr:147]: DSMR telegram line 28: 1-0:71.7.0(001*A)
[15:47:26.128][D][p1_dsmr:147]: DSMR telegram line 29: 1-0:21.7.0(02.763*kW)

[15:47:27.082][D][p1_dsmr:147]: DSMR telegram line 30: 1-0:41.7.0(00.293*kW)
[15:47:27.147][D][p1_dsmr:147]: DSMR telegram line 31: 1-0:61.7.0(00.029*kW)
[15:47:27.147][D][p1_dsmr:147]: DSMR telegram line 32: 1-0:22.7.0(00.000*kW)
[15:47:27.147][D][p1_dsmr:147]: DSMR telegram line 33: 1-0:42.7.0(00.000*kW)
[15:47:27.147][D][p1_dsmr:147]: DSMR telegram line 34: 1-0:62.7.0(00.000*kW)
[15:47:27.147][D][p1_dsmr:147]: DSMR telegram line 35: 0-1:24.1.0(003)
[15:47:27.147][D][p1_dsmr:147]: DSMR telegram line 36: 0-1:96.1.0(----device identifier----)
[15:47:27.147][D][p1_dsmr:147]: DSMR telegram line 37: 0-1:24.2.1(----YYMMDDHHMMSSW----)(08299.581*m3)