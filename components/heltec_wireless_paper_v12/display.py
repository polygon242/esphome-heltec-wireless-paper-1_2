import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import spi, display
from esphome.const import (
    CONF_BUSY_PIN,
    CONF_DC_PIN,
    CONF_FULL_UPDATE_EVERY,
    CONF_ID,
    CONF_LAMBDA,
    CONF_RESET_PIN,
)

DEPENDENCIES = ["spi"]

CONF_BORDER_FULL = "border_full"
CONF_BORDER_FAST = "border_fast"

heltec_wireless_paper_ns = cg.esphome_ns.namespace("heltec_wireless_paper_v12")
HeltecWirelessPaperV12 = heltec_wireless_paper_ns.class_(
    "HeltecWirelessPaperV12",
    cg.PollingComponent,
    spi.SPIDevice,
    display.DisplayBuffer,
)

CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(HeltecWirelessPaperV12),
            cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_BUSY_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_FULL_UPDATE_EVERY, default=15): cv.uint32_t,
            cv.Optional(CONF_BORDER_FULL, default=0x01): cv.hex_uint8_t,
            cv.Optional(CONF_BORDER_FAST, default=0x81): cv.hex_uint8_t,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(spi.spi_device_schema(cs_pin_required=True)),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await display.register_display(var, config)
    await spi.register_spi_device(var, config)

    dc = await cg.gpio_pin_expression(config[CONF_DC_PIN])
    cg.add(var.set_dc_pin(dc))

    if CONF_RESET_PIN in config:
        reset = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset))

    if CONF_BUSY_PIN in config:
        busy = await cg.gpio_pin_expression(config[CONF_BUSY_PIN])
        cg.add(var.set_busy_pin(busy))

    cg.add(var.set_full_update_every(config[CONF_FULL_UPDATE_EVERY]))
    cg.add(var.set_border_full(config[CONF_BORDER_FULL]))
    cg.add(var.set_border_fast(config[CONF_BORDER_FAST]))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA],
            [(display.DisplayRef, "it")],
            return_type=cg.void,
        )
        cg.add(var.set_writer(lambda_))
