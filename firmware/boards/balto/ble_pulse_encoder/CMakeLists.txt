zephyr_library()
zephyr_library_property(ALLOW_EMPTY TRUE)

zephyr_library_sources_ifdef(CONFIG_DT_HAS_INPUT_BOARD_FILTER_ENABLED input_board_filter.c)
