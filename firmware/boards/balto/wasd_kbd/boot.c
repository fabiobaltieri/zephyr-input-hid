#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <soc.h>
#include <stm32_ll_utils.h>

LOG_MODULE_REGISTER(boot, LOG_LEVEL_INF);

uint32_t HAL_SYSTICK_Config(uint32_t TicksNumb)
{
	return SysTick_Config(TicksNumb);
}
void HAL_NVIC_SetPriority(IRQn_Type IRQn, uint32_t PreemptPriority, uint32_t SubPriority)
{
	NVIC_SetPriority(IRQn, PreemptPriority);
}

void jump_to_bootloader(void)
{
	void (*SysMemBootJump)(void);
	uint32_t BootAddr = 0x1fff0000;

	__disable_irq();
	SysTick->CTRL = 0;
	HAL_RCC_DeInit();

	for (uint8_t i = 0; i < ARRAY_SIZE(NVIC->ICER); i++) {
		NVIC->ICER[i] = 0xffffffff;
	}
	for (uint8_t i = 0; i < ARRAY_SIZE(NVIC->ICPR); i++) {
		NVIC->ICPR[i] = 0xffffffff;
	}

	__enable_irq();
	SysMemBootJump = (void (*)(void)) (*((uint32_t *) ((BootAddr + 4))));

	__set_MSP(*(uint32_t *)BootAddr);
	SysMemBootJump();

	for (;;);
}
