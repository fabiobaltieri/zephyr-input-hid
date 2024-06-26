enum blinker_event {
	BLINK_CONNECTED,
	BLINK_DISCONNECTED,
	BLINK_UNPAIRED,
};

#ifdef CONFIG_APP_BLINKER
void blink(enum blinker_event event);
#else
static inline void blink(enum blinker_event event) {};
#endif
