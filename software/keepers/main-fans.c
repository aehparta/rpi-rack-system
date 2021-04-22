
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

	/* reset usart receive bit */
	UCSR0B |= (1 << RXEN0);

	/* adc reference voltage to internal 1.1 V, adjust result left and channel 7 */
	ADMUX = (1 << REFS1) | (1 << REFS0) | (1 << ADLAR) | 0x07;
	/* enable adc, start first conversion and set clock prescaler to full 128 */
	ADCSRA = (1 << ADEN) | (1 << ADSC) | (1 << ADIF) | 0x03;
	/* start first conversion */
	ADCSRA |= (1 << ADSC);

	/* timer 1 to 10Hz */
	TCCR1A = 0x00;
	OCR1A = F_CPU / 256 / 10;
	TCCR1B = (1 << CS12) | (1 << WGM12);

    return 0;
}

int main(void)
{
	p_init();
	asm_main();
	return 0;
}