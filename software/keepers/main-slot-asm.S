
#include <avr/io.h>

#define zero                r0
#define slot_nr             r1

#define miso_buffer_l       r6
#define miso_buffer_h       r7
#define mosi_buffer_l       r8
#define mosi_buffer_h       r9

#define adc_c               r11
#define adc_0               r12
#define adc_1               r13
#define adc_2               r14

; state etc
#define state               r16
#define current_raw         r17
#define card_ok_counter     r18
#define button_pushed       r19

; constant registers used by interrupt routines
; these registers must not be used elsewhere
#define spi_reg_out         r22
#define spi_reg_in          r23

#define tmp0                r24
#define tmp1                r25

; don't use register higher than previous, they are reserved for X, Y and Z

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
    in spi_reg_in, _SFR_IO_ADDR(SPDR)
    out _SFR_IO_ADDR(SPDR), spi_reg_out

    ; load next byte to be send next time
    ld spi_reg_out, X
    ; if byte is zero, then skip incrementing Y
    cpse spi_reg_out, zero
    st X+, zero
    ; buffer is 0x0100-0x07ff, so check when pointer flips to 0x0800 (fourth bit is set)
    sbrc xh, 3
    ldi xh, 0x01

    ; save incoming data to tx buffer
    cpse spi_reg_in, zero
    st Z+, spi_reg_in
    ldi zh, 0x08

    reti


power_state_update_on:
    sbi _SFR_IO_ADDR(PORTB), 0
    sbi LED_PORT, LED_G_BIT
    cbi LED_PORT, LED_R_BIT
    ori state, 0x10
    ret
power_state_update_off:
    cbi _SFR_IO_ADDR(PORTB), 0
    cbi LED_PORT, LED_G_BIT
    sbi LED_PORT, LED_R_BIT
    andi state, ~0x10
    ret


.global asm_main
asm_main:
    ; initialize compare value registers
    clr zero

    ; initialize buffer pointers
    ldi xh, 0x01
    clr xl
    ldi yh, 0x01
    clr yl
    ldi zh, 0x08
    clr zl
    movw miso_buffer_l, yl
    movw mosi_buffer_l, zl
ram_clear:
    st X+, zero
    cpse xh, 0x09
    rjmp ram_clear
    ldi xh, 0x01

    ; read  slot number from eeprom
    out _SFR_IO_ADDR(EEARH), zero
    out _SFR_IO_ADDR(EEARL), zero
    sbi _SFR_IO_ADDR(EECR), EERE
    in slot_nr, _SFR_IO_ADDR(EEDR)

    ; read  default status from eeprom
    out _SFR_IO_ADDR(EEARH), zero
    ldi tmp0, 1
    out _SFR_IO_ADDR(EEARL), tmp0
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
    ori state, 0x11
    cpse slot_nr, zero
    andi state, ~0x11

    ; spi interrupt related registers
    clr spi_reg_in
    clr spi_reg_out

    ; button pushed counter
    clr button_pushed

    ; card disconnect detection timer
    clr card_ok_counter

    ; ADC */
    clr adc_c
    clr adc_0
    clr adc_1
    clr adc_2
    ser current_raw

    ; enable interrupts 
    sei

    ; loop here forevah!
main_loop:
    wdr ; watchdog reset

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
    lds tmp0, ADCL
    add adc_0, tmp0
    lds tmp0, ADCH
    adc adc_1, tmp0
    adc adc_2, zero
    inc adc_c
    brvc adc_handler_start_again
    ; save measurement average
    mov current_raw, adc_2
    clr adc_0
    clr adc_1
    clr adc_2
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
    ; put state and raw current into send queue
    movw yl, miso_buffer_l
    ldi tmp1, 0x11
    st Y+, tmp1
    sbrc yh, 3
    ldi yh, 0x01
    st Y+, state
    sbrc yh, 3
    ldi yh, 0x01
    ldi tmp1, 0x12
    st Y+, tmp1
    sbrc yh, 3
    ldi yh, 0x01
    st Y+, current_raw
    sbrc yh, 3
    ldi yh, 0x01
    movw miso_buffer_l, yl

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
    rcall power_state_update_on
button_handler_clear:
    ser button_pushed
button_handler_pushed:
    inc button_pushed
    ; if button has been pushed for 4 seconds, force power off
    cpi button_pushed, 40
    brne button_handler_end
    rcall power_state_update_off
button_handler_end:
    ;
timer_handler_end:

    ;
    ; skip all stuff from here on if no card is connected
    ;
    sbrs state, 6
    rjmp main_loop

    ; uart handling
uart_handlers:
    ; load uart status into tmp1
    lds tmp1, UCSR0A

    ; check if we should and can send data to uart
mosi_buffer_handler:
    sbrs tmp1, UDRE0
    rjmp mosi_buffer_handler_end
    movw yl, mosi_buffer_l
    ld tmp0, Y
    cpse tmp0, zero
    rcall mosi_buffer_handler_has_data
    movw mosi_buffer_l, yl
mosi_buffer_handler_end:

    ; check if new data from uart has been received
miso_buffer_handler:
    sbrs tmp1, RXC0
    rjmp miso_buffer_handler_end
    movw yl, miso_buffer_l
    ; receive byte from uart
    lds tmp0, UDR0
    cpse tmp0, zero
    st Y+, tmp0
    ; buffer is 0x0100-0x07ff, so check when pointer flips to 0x0800 (fourth bit is set)
    sbrc yh, 3
    ldi yh, 0x01
    movw miso_buffer_l, yl
miso_buffer_handler_end:

    rjmp main_loop


mosi_buffer_handler_has_data:
    ; clear current position
    st Y, zero
    ; increase buffer pointer
    inc yl
    ; power on command check
    cpi tmp0, 0x05
    brne mosi_buffer_handler_has_data__not_power_on
    ; don't accept power commands for slot 0 from remote
    cpse slot_nr, zero
    rcall power_state_update_on
    ret
mosi_buffer_handler_has_data__not_power_on:
    ; power off command check
    cpi tmp0, 0x04
    brne mosi_buffer_handler_has_data__not_power_off
    ; don't accept power commands for slot 0 from remote
    cpse slot_nr, zero
    rcall power_state_update_off
    ret
mosi_buffer_handler_has_data__not_power_off:
    ; sent to uart
    sts UDR0, tmp0
    ret
