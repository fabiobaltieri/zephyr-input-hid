#!/usr/bin/env python3

import usb

dev = usb.core.find(product="WASD Keyboard")
if dev is None:
    raise ValueError("Device not found")

print(f"Board serial: {dev.serial_number}")

req = 0
index = 0xfffe
value = 0

TO_DEV_REQ_TYPE = 0x40
TO_HOST_REQ_TYPE = 0xC0

try:
    dev.ctrl_transfer(TO_DEV_REQ_TYPE, req, value, index, 0)
except usb.USBError as e:
    pass
