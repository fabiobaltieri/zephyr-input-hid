enum event_code {
	EVENT_BOOT,
	EVENT_INPUT,
	EVENT_BT_CONNECTED,
	EVENT_BT_DISCONNECTED,
	EVENT_BT_UNPAIRED,
	EVENT_USB_CONNECTED,
	EVENT_USB_DISCONNECTED,
	EVENT_HID_RESUMED,
	EVENT_HID_SUSPENDED,
	EVENT_CHARGER_CHARGING,
	EVENT_CHARGER_FULL,
	EVENT_CHARGER_DISCONNECTED,
	EVENT_POWEROFF,
};

struct event_callback {
	void (*callback)(enum event_code);
};

void event(enum event_code code);

#define EVENT_CALLBACK_DEFINE(_callback)					\
	static const STRUCT_SECTION_ITERABLE(event_callback,			\
					     _event_callback__##_callback) = {	\
		.callback = _callback,						\
        }
