board_runner_args(jlink "--device=nRF52832_xxAA" "--speed=4000")
board_runner_args(nrfjprog "--softreset")

include(${ZEPHYR_BASE}/boards/common/nrfjprog.board.cmake)
include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
include(${ZEPHYR_BASE}/boards/common/blackmagicprobe.board.cmake)
