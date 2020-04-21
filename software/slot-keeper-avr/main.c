/*
 * Fuses: -U lfuse:w:0xde:m -U hfuse:w:0xd9:m -U efuse:w:0xff:m
 */

#include <libe/libe.h>

extern void asm_main(void);

int p_init(void)
{
	/* library initializations */
	os_init();
	log_init();
	nvm_init(NULL, 0);

	/* enable SPI as slave */
	gpio_output(GPIOB4); /* MISO must be set as output */
	SPCR = (1 << SPIE) | (1 << SPE);
	// SPCR = (1 << SPE);

	/* enable usart receive with interrupt */
	// UCSR0B |= (1 << RXEN0) | (1 << RXCIE0);
	UCSR0B |= (1 << RXEN0);

	return 0;
}

int main(void)
{
	ERROR_IF_R(p_init(), -1, "base initialization failed");
	asm_main();
	return 0;
}
