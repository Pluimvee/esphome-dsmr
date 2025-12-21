"""ESPHome component: minimal DSMR/P1 reader that maps OBIS codes to sensors."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, sensor
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_UPDATE_INTERVAL,
    CONF_UART_ID,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_KILOWATT_HOURS,
    UNIT_WATT,
)

CODEOWNERS = ["@Pluimvee"]
DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor", "uart"]

ns = cg.esphome_ns.namespace("p1_dsmr")
P1Dsmr = ns.class_("P1DsmrComponent", cg.PollingComponent, uart.UARTDevice)

CONF_SENSORS = "sensors"
CONF_OBIS = "obis"
CONF_OBIS_VALIDITY = "obis_validity"

OBIS_SENSOR_SCHEMA = sensor.sensor_schema(
    accuracy_decimals=3,
    icon="mdi:counter",
).extend({cv.Required(CONF_OBIS): cv.string_strict})

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(P1Dsmr),
            cv.Required(CONF_UART_ID): cv.use_id(uart.UARTComponent),
            cv.Optional(CONF_OBIS_VALIDITY, default="10s"): cv.update_interval,
            cv.Optional(CONF_SENSORS, default=[]): cv.ensure_list(OBIS_SENSOR_SCHEMA),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.polling_component_schema("500ms"))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_obis_validity(config[CONF_OBIS_VALIDITY]))

    for sens_conf in config[CONF_SENSORS]:
        sens = await sensor.new_sensor(sens_conf)
        cg.add(var.register_obis_sensor(sens_conf[CONF_OBIS], sens))
