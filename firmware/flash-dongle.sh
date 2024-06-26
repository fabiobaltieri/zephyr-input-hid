#!/bin/bash

nrfutil pkg generate --hw-version 52 --sd-req=0x00 \
	--application build/zephyr/zephyr.hex \
	--application-version 1 build/out.zip

nrfutil dfu usb-serial -pkg build/out.zip -p /dev/ttyACM0
