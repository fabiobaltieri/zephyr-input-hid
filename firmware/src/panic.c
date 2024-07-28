#include <zephyr/fatal.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/reboot.h>

LOG_MODULE_REGISTER(panic, LOG_LEVEL_INF);

void k_sys_fatal_error_handler(unsigned int reason,
			       const struct arch_esf *esf)
{
	LOG_PANIC();
	LOG_ERR("Fatal error - reboot");
	sys_reboot(SYS_REBOOT_COLD);
}
