
#include <avr/io.h>

#define tmp0                r0
#define zero                r1
#define one                 r2
#define two                 r3

#define adc_c               r11
#define adc_l               r12
#define adc_h               r13
#define spi_data_lock       r14
#define DC1                 r15

; state etc
#define slot_nr             r16
#define state               r17
#define current_raw         r18
#define state_set           r19
#define card_ok_counter     r20
#define button_pushed       r21

; constant registers used by interrupt routines
; these registers must not be used elsewhere
#define spi_reg_out         r23
#define spi_reg_in          r24

; don't use register higher than this, they are reserved
#define tmp1                r25

; button
#define BUTTON_PORT         _SFR_IO_ADDR(PIND)
#define BUTTON_BIT          7

; LEDs
#define LED_PORT            _SFR_IO_ADDR(PORTD)
#define LED_R_BIT           4
#define LED_G_BIT           5
#define LED_B_BIT           6

; card
#define CARD_DETECT_PORT    _SFR_IO_ADDR(PINC)
#define CARD_DETECT_PIN     2

; spi data received interrupt
.global SPI_STC_vect
SPI_STC_vect:
    ; read incoming data
    in spi_reg_in, _SFR_IO_ADDR(SPDR)
    ; send incoming uart data or zero if locked
    sbrs spi_data_lock, 0
    ld spi_reg_out, Y
    out _SFR_IO_ADDR(SPDR), spi_reg_out
    ; if byte is zero, then skip incrementing Y
    cpse spi_reg_out, zero
    st Y+, zero
    ; always clear out reg
    clr spi_reg_out
    ; buffer is 0x0100-0x07ff, so check when Y flips to 0x0800
    sbrc yh, 3
    ldi yh, 0x01
    ; check for incoming commands
    cpi spi_reg_in, 8
    brlo spi_is_command
    cpse spi_reg_in, DC1
    sts UDR0, spi_reg_in
    reti
spi_is_command:
    mov state_set, spi_reg_in
    reti


power_state_update:
    ; enable/disable card power
    sbrs state_set, 0
    rjmp power_state_update_off
power_state_update_on:
    sbi _SFR_IO_ADDR(PORTB), 0
    sbi LED_PORT, LED_G_BIT
    cbi LED_PORT, LED_R_BIT
    ori state, 0x10
    clr state_set
    ret
power_state_update_off:
    cbi _SFR_IO_ADDR(PORTB), 0
    cbi LED_PORT, LED_G_BIT
    sbi LED_PORT, LED_R_BIT
    andi state, ~0x10
    clr state_set
    ret


.global asm_main
asm_main:
    ; initialize compare value registers
    clr zero
    ldi tmp1, 0x01
    mov one, tmp1
    ldi tmp1, 0x02
    mov two, tmp1
    ldi tmp1, 0x11
    mov DC1, tmp1

    ; initialize buffer pointers in X and Y
    ldi xh, 0x01
    ldi xl, 0x00
    ldi yh, 0x01
    ldi yl, 0x00
clear_buffer:
    st Y+, zero
    sbrs yh, 3
    rjmp clear_buffer
    ldi yh, 0x01
    ldi yl, 0x00

    ; read  slot number from eeprom
    out _SFR_IO_ADDR(EEARH), zero
    out _SFR_IO_ADDR(EEARL), zero
    sbi _SFR_IO_ADDR(EECR), EERE
    in slot_nr, _SFR_IO_ADDR(EEDR)

    ; read  default status from eeprom
    out _SFR_IO_ADDR(EEARH), zero
    out _SFR_IO_ADDR(EEARL), one
    sbi _SFR_IO_ADDR(EECR), EERE
    in state, _SFR_IO_ADDR(EEDR)
    ; only lower bits are used
    andi state, 0x0f
    ; set bit 7 to indicate status is ok and avr is detected right on master
    ori state, 0x80
    ; set bit 4 if slot is powered on as default (done in C side)
    sbrc state, 0
    ori state, 0x10
    ; set bit 4 (and 1) also if it is slot 0, it is on as default
    cp slot_nr, zero
    brne init_not_slot_zero
    ori state, 0x11
init_not_slot_zero:

    ; spi interrupts related registers
    clr spi_reg_in
    clr spi_reg_out
    clr spi_data_lock

    ; button pushed counter
    clr button_pushed

    ; card disconnect detection timer
    clr card_ok_counter

    ; ADC */
    clr adc_c
    clr adc_l
    clr adc_h
    ser current_raw

    ; clear state set
    clr state_set

    ; enable interrupts 
    sei

    ; loop here forevah!
main_loop:
    wdr ; watchdog reset

    ; check incoming state change request
state_action:
    sbrs state_set, 2
    rjmp state_action_end
    ; skip any state changes also if slot zero
    cp slot_nr, zero
    breq state_action_end
    call power_state_update
state_action_end:

    ; card detection
card_detect:
    sbic CARD_DETECT_PORT, CARD_DETECT_PIN
    rjmp card_detect_no
    ori state, 0x40
    ser card_ok_counter
    rjmp card_detect_end
card_detect_no:
    andi state, ~0x40
    dec card_ok_counter
    ; disable power if zero, card was removed
    brne card_detect_end
    ; do not turn off power on slot zero
    cpse slot_nr, zero
    cbi _SFR_IO_ADDR(PORTB), 0
    cbi LED_PORT, LED_G_BIT
    sbi LED_PORT, LED_R_BIT
    andi state, ~0x10
card_detect_end:

    ; read adc
adc_handler:
    lds tmp1, ADCSRA
    sbrs tmp1, ADIF
    rjmp adc_handler_end
    lds tmp0, ADCH
    add adc_l, tmp0
    adc adc_h, zero
    inc adc_c
    brvc adc_handler_start_again
    ; save measurement average
    mov current_raw, adc_h
    clr adc_l
    clr adc_h
    ; cannot send zero, set to 0xff if zero
    cp current_raw, zero
    brne adc_handler_start_again
    ser current_raw
adc_handler_start_again:
    ; clear interrupt and start next conversion
    ori tmp1, 0x50
    sts ADCSRA, tmp1
adc_handler_end:

    ; timer 1, 100Hz
timer_handler:
    sbis _SFR_IO_ADDR(TIFR1), OCF1A
    rjmp timer_handler_end
    ldi tmp1, 0x07
    out _SFR_IO_ADDR(TIFR1), tmp1
    or spi_data_lock, one
    ; put state and raw current into send queue
    ldi tmp1, 0x11
    st X+, tmp1
    sbrc xh, 3
    ldi xh, 0x01
    st X+, state
    sbrc xh, 3
    ldi xh, 0x01
    ldi tmp1, 0x12
    st X+, tmp1
    sbrc xh, 3
    ldi xh, 0x01
    st X+, current_raw
    sbrc xh, 3
    ldi xh, 0x01
    clr spi_data_lock
    ; check button (inside timer)
button_handler:
    sbis BUTTON_PORT, BUTTON_BIT
    rjmp button_handler_pushed
button_handler_not_pushed:
    ; if button was not even down before
    cpi button_pushed, 0
    breq button_handler_end
    ; check power state, if power is off, turn on
    sbrc state, 4
    rjmp button_handler_clear
    ; turn power on only if this was short push, less than 500ms
    cpi button_pushed, 5
    brsh button_handler_clear
    call power_state_update_on
button_handler_clear:
    ser button_pushed
button_handler_pushed:
    inc button_pushed
    ; if button has been pushed for 4 seconds, force power off
    cpi button_pushed, 40
    brne button_handler_end
    call power_state_update_off
button_handler_end:
    ;
timer_handler_end:

    ; check if new data from uart has been received only if a card is connected
uart_rx_handler:
    sbrs state, 6
    rjmp uart_rx_handler_end
    lds tmp1, UCSR0A
    sbrs tmp1, RXC0
    rjmp uart_rx_handler_end
    ; receive byte from uart
    lds tmp1, UDR0
    cpse tmp1, zero
    st X+, tmp1
    ; buffer is 0x0100-0x07ff, so check when X flips to 0x0800
    sbrc xh, 3
    ldi xh, 0x01
uart_rx_handler_end:

    rjmp main_loop
