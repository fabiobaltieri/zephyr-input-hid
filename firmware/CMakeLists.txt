cmake_minimum_required(VERSION 3.20.0)

set(BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR})
set(BOARD kbd_matrix)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(keyboard-matrix-testboard)

target_sources(app PRIVATE
	src/hid_kbd.c
	src/hid_kbd.h
	src/hid_report_map.c
	src/hid_report_map.h
	src/main.c
)

target_sources_ifdef(CONFIG_BT app PRIVATE
	src/ble.c
	src/ble_hog.c
	src/ble_hog.h
)
