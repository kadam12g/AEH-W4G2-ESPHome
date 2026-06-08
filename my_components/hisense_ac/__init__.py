import esphome.codegen as cg
from esphome.components import uart, climate

DEPENDENCIES = ["uart"]

hisense_ac_ns = cg.esphome_ns.namespace("hisense_ac")
HisenseAC = hisense_ac_ns.class_("HisenseAC", uart.UARTDevice, cg.Component, climate.Climate)