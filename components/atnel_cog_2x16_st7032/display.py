import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import display, i2c
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_UPDATE_INTERVAL, CONF_RESET_PIN
from esphome import pins

DEPENDENCIES = ["i2c"]

CONF_CONTRAST = "contrast"
CONF_COLUMNS = "columns"
CONF_ROWS = "rows"
CONF_USER_CHARACTERS = "user_characters"
CONF_POSITION = "position"
CONF_DATA = "data"

atnel_cog_ns = cg.esphome_ns.namespace("atnel_cog_2x16_st7032")
AtnelCogDisplay = atnel_cog_ns.class_(
    "AtnelCogDisplay", display.Display, i2c.I2CDevice
)

# User-defined characters: max 8 chars (positions 0-7), each 8 bytes of 5-bit data
USER_CHARACTER_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_POSITION): cv.int_range(min=0, max=7),
        cv.Required(CONF_DATA): cv.All(
            cv.ensure_list(cv.int_range(min=0, max=0b11111)),
            cv.Length(min=8, max=8),
        ),
    }
)

CONFIG_SCHEMA = (
    display.BASIC_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(AtnelCogDisplay),
            cv.Optional(CONF_COLUMNS, default=16): cv.int_range(min=8, max=40),
            cv.Optional(CONF_ROWS, default=2): cv.int_range(min=1, max=4),
            cv.Optional(CONF_CONTRAST, default=32): cv.int_range(min=0, max=63),
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_USER_CHARACTERS): cv.ensure_list(USER_CHARACTER_SCHEMA),
        }
    )
    .extend(cv.polling_component_schema("5s"))
    .extend(i2c.i2c_device_schema(0x3E))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await display.register_display(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_columns(config[CONF_COLUMNS]))
    cg.add(var.set_rows(config[CONF_ROWS]))
    cg.add(var.set_contrast(config[CONF_CONTRAST]))

    if CONF_RESET_PIN in config:
        reset = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset))

    if CONF_USER_CHARACTERS in config:
        for char_conf in config[CONF_USER_CHARACTERS]:
            pos = char_conf[CONF_POSITION]
            data = char_conf[CONF_DATA]
            cg.add(var.set_user_character(pos, data))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA],
            [(AtnelCogDisplay.operator("ref"), "it")],
            return_type=cg.void,
        )
        cg.add(var.set_writer(lambda_))
