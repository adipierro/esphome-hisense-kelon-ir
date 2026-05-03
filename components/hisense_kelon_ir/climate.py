import esphome.codegen as cg
from esphome import automation
from esphome.components import climate, climate_ir, remote_base
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TEMPERATURE

from . import hisense_kelon_ir_ns

CODEOWNERS = ["@dipierro"]
AUTO_LOAD = ["climate_ir", "remote_base"]
DEPENDENCIES = ["remote_transmitter"]

CONF_ENABLED = "enabled"
CONF_ENSURE_POWER = "ensure_power"
CONF_ENSURE_POWER_ON_BOOT = "ensure_power_on_boot"

EnsurePowerMode = hisense_kelon_ir_ns.enum("EnsurePowerMode")
ENSURE_POWER_MODES = {
    "smart": EnsurePowerMode.ENSURE_POWER_SMART,
    "super": EnsurePowerMode.ENSURE_POWER_SUPER,
    "none": EnsurePowerMode.ENSURE_POWER_NONE,
}

HisenseKelonIRClimate = hisense_kelon_ir_ns.class_(
    "HisenseKelonIRClimate", climate_ir.ClimateIR
)
FollowMeAction = hisense_kelon_ir_ns.class_("FollowMeAction", automation.Action)
DisplayOffAction = hisense_kelon_ir_ns.class_("DisplayOffAction", automation.Action)
Kelon168Dumper = hisense_kelon_ir_ns.class_(
    "Kelon168Dumper", remote_base.RemoteReceiverDumperBase
)


@remote_base.register_dumper("hisense_kelon_ir", Kelon168Dumper)
def hisense_kelon_ir_dumper(var, config):
    pass

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(HisenseKelonIRClimate).extend(
    {
        cv.Optional(CONF_ENSURE_POWER, default="smart"): cv.enum(
            ENSURE_POWER_MODES, lower=True
        ),
        cv.Optional(CONF_ENSURE_POWER_ON_BOOT, default=False): cv.boolean,
    }
)


async def to_code(config):
    var = await climate_ir.new_climate_ir(config)
    cg.add(var.set_ensure_power(config[CONF_ENSURE_POWER]))
    cg.add(var.set_ensure_power_on_boot(config[CONF_ENSURE_POWER_ON_BOOT]))


FOLLOW_ME_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(HisenseKelonIRClimate),
        cv.Required(CONF_TEMPERATURE): cv.templatable(cv.float_),
        cv.Optional(CONF_ENABLED, default=True): cv.templatable(cv.boolean),
    }
)


@automation.register_action(
    "hisense_kelon_ir.follow_me",
    FollowMeAction,
    FOLLOW_ME_SCHEMA,
    synchronous=True,
)
async def follow_me_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    template_ = await cg.templatable(config[CONF_TEMPERATURE], args, cg.float_)
    cg.add(var.set_temperature(template_))
    template_ = await cg.templatable(config[CONF_ENABLED], args, bool)
    cg.add(var.set_enabled(template_))
    return var


DISPLAY_OFF_SCHEMA = cv.Schema({cv.Required(CONF_ID): cv.use_id(HisenseKelonIRClimate)})


@automation.register_action(
    "hisense_kelon_ir.display_off",
    DisplayOffAction,
    DISPLAY_OFF_SCHEMA,
    synchronous=True,
)
async def display_off_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)
