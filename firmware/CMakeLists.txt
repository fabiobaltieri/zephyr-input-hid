cmake_minimum_required(VERSION 3.20.0)

set(BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR})
set(BOARD kbd_matrix)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(keyboard-matrix-testboard)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})
