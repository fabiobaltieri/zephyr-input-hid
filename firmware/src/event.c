#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/iterable_sections.h>

#include "event.h"

LOG_MODULE_REGISTER(event, LOG_LEVEL_INF);

void event(enum event_code code)
{
	STRUCT_SECTION_FOREACH(event_callback, callback) {
		callback->callback(code);
	}
}
