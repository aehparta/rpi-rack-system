/*
 * Use: avrdude -c linuxspi -p atmega328p -P /dev/spidev0.0 -b 100000
 * Fuses: -U lfuse:w:0xe0:m -U hfuse:w:0xd1:m -U efuse:w:0xff:m
 */

#include <libe/libe.h>

extern void asm_main(void);

/* AVR <-> holder
 * PB0 = Power
 * PC0 = PI shutdown
 * PC1 = PI power off
 * PC2 = holder card inserted (low, otherwise floating)
 * PC3 = PI heartbeat
 * PD4 = LED R
 * PD5 = LED G
 * PD6 = LED B
 * PD7 = BUTTON (has pull-up)
 */

#define POWER GPIOB0

#define CARD_INSERTED GPIOC2
#define SHUTDOWN_REQUEST GPIOC0
#define POWER_OFF_SIGNAL GPIOC1

#define LED_R GPIOD4
#define LED_G GPIOD5
#define LED_B GPIOD6
#define BUTTON GPIOD7

int p_init(void)
{
	/* library initializations */
	os_init();
	log_init();
	nvm_init(NULL, 0);

	/* read slot number and defaults */
	uint8_t slot_nr = nvm_read_byte((uint8_t *)0, 0);
	uint8_t default_status = nvm_read_byte((uint8_t *)1, 0);
	gpio_output(POWER);
	if (default_status & 1 || slot_nr == 0) {
		/* on as default */
		gpio_high(POWER);
	} else {
		/* off as default */
		gpio_low(POWER);
	}

	/* enable SPI as slave */
	gpio_output(GPIOB4); /* MISO must be set as output */
	SPCR = (1 << SPIE) | (1 << SPE);

	/* reset uart receive bit */
	UCSR0B |= (1 << RXEN0);

	/* LEDs */
	gpio_output(LED_R);
	gpio_high(LED_R);
	os_delay_ms(500);
	gpio_low(LED_R);
	gpio_output(LED_G);
	gpio_high(LED_G);
	os_delay_ms(500);
	gpio_low(LED_G);
	gpio_output(LED_B);
	gpio_high(LED_B);
	os_delay_ms(500);
	gpio_low(LED_B);
	if (default_status & 1 || slot_nr == 0) {
		gpio_high(LED_G);
	} else {
		gpio_high(LED_R);
	}

	/* button */
	gpio_input(BUTTON);

	/* card inserted detection (floating so enable internal pull-up) */
	gpio_input(CARD_INSERTED);
	gpio_high(CARD_INSERTED);

	/* as inputs with pull-up for now */
	gpio_input(SHUTDOWN_REQUEST);
	gpio_high(SHUTDOWN_REQUEST);
	gpio_input(POWER_OFF_SIGNAL);
	gpio_high(POWER_OFF_SIGNAL);

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
