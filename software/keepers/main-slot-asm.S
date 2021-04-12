
#include <avr/io.h>

#define zero                r1
#define one                 r2
#define two                 r3

; status etc
#define slot_nr             r16
#define status_1            r17
#define status_2            r18
#define state_set           r19

; constant registers used by interrupt routines
; these registers must not be used elsewhere
#define spi_reg_out         r23
#define spi_reg_in          r24

; don't use register higher than this, they are reserved
#define tmp                 r25


; spi data received interrupt
.global SPI_STC_vect
SPI_STC_vect:
    in spi_reg_in, _SFR_IO_ADDR(SPDR)
    ; if 0x01
    cpse spi_reg_in, one
    rjmp not_status_1
    ; send status 1
    out _SFR_IO_ADDR(SPDR), status_1
    reti
not_status_1:
    ; if 0x02
    cpse spi_reg_in, two
    rjmp not_status_2
    ; send status 2
    out _SFR_IO_ADDR(SPDR), status_2
    reti
not_status_2:
    ; if < 8
    cpi spi_reg_in, 0
    breq process_spi_data
    cpi spi_reg_in, 8
    brsh process_spi_data
    mov state_set, spi_reg_in
    clr spi_reg_in
process_spi_data:
    ; send incoming uart data
    ld spi_reg_out, Y
    out _SFR_IO_ADDR(SPDR), spi_reg_out
    ; if byte is zero, then skip incrementing Y
    cpse spi_reg_out, zero
    ld spi_reg_out, Y+
    ; buffer is 0x0100-0x07ff, so check when Y flips to 0x0800
    sbrc yh, 3
    ldi yh, 0x01
    ; if incoming command was not zero, send to uart
    cpse spi_reg_in, zero
    sts UDR0, spi_reg_in
    reti


.global asm_main
asm_main:
    ; initialize compare value registers
    clr zero
    ldi tmp, 0x01
    mov one, tmp
    ldi tmp, 0x02
    mov two, tmp

    ; initialize buffer pointers in X and Y
    ldi xh, 0x02
    ldi xl, 0x00
    ldi yh, 0x02
    ldi yl, 0x00
    st Y, zero

    ; read  slot number from eeprom
    out _SFR_IO_ADDR(EEARH), zero
    out _SFR_IO_ADDR(EEARL), zero
    sbi _SFR_IO_ADDR(EECR), EERE
    in slot_nr, _SFR_IO_ADDR(EEDR)

    ; read  default status from eeprom
    out _SFR_IO_ADDR(EEARH), zero
    out _SFR_IO_ADDR(EEARL), one
    sbi _SFR_IO_ADDR(EECR), EERE
    in status_1, _SFR_IO_ADDR(EEDR)
    ; only lower bits are used
    andi status_1, 0x0f
    ; set bit 7 to indicate status is ok and avr is detected right on master
    ori status_1, 0x80
    ; set bit 4 if slot is powered on as default (done in C side)
    sbrc status_1, 0
    ori status_1, 0x10
    ; set bit 4 (and 1) also if it is slot 0, it is on as default
    cp slot_nr, zero
    brne init_not_slot_zero
    ori status_1, 0x11
init_not_slot_zero:

    ; status 2 is used raw value read from ADC */
    ldi status_2, 0x00

    ; enable interrupts 
    sei

    ; loop here forevah!
main_loop:
    wdr ; watchdog reset

    ; check incoming state change request
state_action:
    sbrs state_set, 2
    rjmp state_no_action
    ; skip any state changes also if slot zero
    cp slot_nr, zero
    breq state_no_action
    ; set power state 
    sbrc state_set, 0
    sbi _SFR_IO_ADDR(PORTB), 0
    sbrs state_set, 0
    cbi _SFR_IO_ADDR(PORTB), 0
    sbrc state_set, 0
    ori status_1, 0x10
    sbrs state_set, 0
    andi status_1, ~0x10
    ; write default power on state
    ; TODO
    clr state_set
state_no_action:

    ; check card inserted
card_inserted:
    sbic _SFR_IO_ADDR(PINC), 2
    andi status_1, ~0x40
    sbis _SFR_IO_ADDR(PINC), 2
    ori status_1, 0x40

    ; read adc
adc_handler:
    lds tmp, ADCSRA
    sbrs tmp, ADIF
    rjmp adc_handler_end
    lds status_2, ADCH
    ; clear interrupt and start next conversion
    ori tmp, 0x50
    sts ADCSRA, tmp
adc_handler_end:

    ; check button
button_handler:
    cbi _SFR_IO_ADDR(PORTD), 6
    sbic _SFR_IO_ADDR(PIND), 7
    rjmp button_handler_end
    sbi _SFR_IO_ADDR(PORTD), 6
button_handler_end:

    ; check if new data from uart has been received
uart_rx_handler:
    lds tmp, UCSR0A
    sbrs tmp, RXC0
    rjmp uart_rx_handler_end
    ; receive byte from uart
    lds tmp, UDR0
    st X+, tmp
    ; buffer is 0x0100-0x07ff, so check when X flips to 0x0800
    sbrc xh, 3
    ldi xh, 0x01
    ; save zero to current index so reading will know where to stop
    st X, zero
uart_rx_handler_end:

    rjmp main_loop
