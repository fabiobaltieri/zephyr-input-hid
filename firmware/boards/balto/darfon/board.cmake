board_runner_args(dfu-util "--pid=1d50:615e" "--alt=0" "--dfuse")
board_runner_args(blackmagicprobe "--gdb-serial=/dev/ttyBmpGdb" "--connect-srst")
board_runner_args(jlink "--device=STM32F103RC" "--speed=4000")

include(${ZEPHYR_BASE}/boards/common/dfu-util.board.cmake)
include(${ZEPHYR_BASE}/boards/common/blackmagicprobe.board.cmake)
include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
