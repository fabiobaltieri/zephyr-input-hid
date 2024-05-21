cmake_minimum_required(VERSION 3.20.0)

set(BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR})
set(BOARD kbd_matrix)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(keyboard-matrix-testboard)

target_include_directories(app PRIVATE include)

target_sources(app PRIVATE
	src/main.c
)

target_sources_ifdef(CONFIG_BT app PRIVATE src/ble.c)

target_sources_ifdef(CONFIG_DT_HAS_HID_ENABLED app PRIVATE src/hid.c)
target_sources_ifdef(CONFIG_DT_HAS_HID_KBD_ENABLED app PRIVATE src/hid_kbd.c)
target_sources_ifdef(CONFIG_DT_HAS_HID_HOG_ENABLED app PRIVATE src/ble_hog.c)
target_sources_ifdef(CONFIG_DT_HAS_USB_HID_ENABLED app PRIVATE src/usb_hid.c)
