cmake_minimum_required(VERSION 3.20.0)

set(BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR})
set(BOARD kbd_matrix)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(keyboard-matrix-testboard)

zephyr_include_directories(include)

target_sources(app PRIVATE
	src/main.c
	src/event.c
	src/panic.c
)

zephyr_linker_sources(ROM_SECTIONS src/event.ld)

target_sources_ifdef(CONFIG_BT app PRIVATE src/ble.c src/ble_status.c src/ble_shell.c)

target_sources_ifdef(CONFIG_APP_BAS_FUEL_GAUGE app PRIVATE src/bas_fuel_gauge.c)
target_sources_ifdef(CONFIG_APP_BAS_FUEL_GAUGE_SENSOR app PRIVATE src/bas_fuel_gauge_sensor.c)
target_sources_ifdef(CONFIG_APP_CHARGER app PRIVATE src/charger.c)
target_sources_ifdef(CONFIG_APP_CHARGER_GPIO app PRIVATE src/charger_gpio.c)
target_sources_ifdef(CONFIG_APP_AUTO_POWEROFF app PRIVATE src/auto_poweroff.c)
target_sources_ifdef(CONFIG_INPUT_ANALOG_AXIS app PRIVATE src/analog-axis-shell.c)

target_sources_ifdef(CONFIG_DT_HAS_HID_ENABLED app PRIVATE src/hid.c)
target_sources_ifdef(CONFIG_DT_HAS_HID_KBD_ENABLED app PRIVATE src/hid_kbd.c)
target_sources_ifdef(CONFIG_DT_HAS_HID_MOUSE_ENABLED app PRIVATE src/hid_mouse.c)
target_sources_ifdef(CONFIG_DT_HAS_HID_GAMEPAD_ENABLED app PRIVATE src/hid_gamepad.c)
target_sources_ifdef(CONFIG_DT_HAS_HID_HOG_ENABLED app PRIVATE src/ble_hog.c src/ble_hog_shell.c)
target_sources_ifdef(CONFIG_DT_HAS_USB_HID_ENABLED app PRIVATE src/usb_hid.c)
target_sources_ifdef(CONFIG_DT_HAS_BLE_UNPAIR_ENABLED app PRIVATE src/ble_unpair.c)
target_sources_ifdef(CONFIG_DT_HAS_XINPUT_ENABLED app PRIVATE src/xinput.c)
target_sources_ifdef(CONFIG_DT_HAS_TI_DRV8830_ENABLED app PRIVATE src/drv8830.c)

target_sources_ifdef(CONFIG_USB_DEVICE_STACK_NEXT app PRIVATE src/usbd.c)
