
#include <avr/io.h>

#define zero                r0
#define slot_nr             r1
#define sreg_int            r2

#define miso_buffer_l       r6
#define miso_buffer_h       r7
#define mosi_buffer_l       r8
#define mosi_buffer_h       r9

#define adc_c               r11
#define adc_0               r12
#define adc_1               r13
#define adc_2               r14

; state etc
#define fans                r16
#define fans_pwm_value      r17
#define fans_pwm_counter    r18
#define U_raw               r19
#define last_byte           r20

; constant registers used by interrupt routines
; these registers must not be used elsewhere
#define spi_reg_out         r22
#define spi_reg_in          r23

#define tmp0                r24
#define tmp1                r25

; don't use register higher than previous, they are reserved for X, Y and Z

#define FANS_PORT           _SFR_IO_ADDR(PORTC)
#define FANS_DDR            _SFR_IO_ADDR(DDRC)
#define FAN0_BIT            0
#define FAN1_BIT            1
#define FAN2_BIT            2
#define FAN3_BIT            3
#define FAN4_BIT            4
#define FAN5_BIT            5

; spi data received interrupt
.global SPI_STC_vect
SPI_STC_vect:
    in sreg_int, _SFR_IO_ADDR(SREG)
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

    out _SFR_IO_ADDR(SREG), sreg_int
    reti


.global asm_main
asm_main:
    ; initialize compare value registers
    clr zero

    ; fans gpio as outputs and on as start
    ldi tmp0, 0x3f
    out FANS_DDR, tmp0
    out FANS_PORT, zero

    ; fans state and pwm
    ldi fans, 0x00
    ldi fans_pwm_value, 0x07
    clr fans_pwm_counter

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

    ; spi interrupt related registers
    clr spi_reg_in
    clr spi_reg_out

    ; previous byte that could be a command
    clr last_byte

    ; ADC */
    clr adc_c
    clr adc_0
    clr adc_1
    clr adc_2
    ser U_raw

    ; enable interrupts 
    sei

    ; loop here forevah!
main_loop:
    wdr ; watchdog reset

    ; fan software pwm
fans_pwm_handler:
    inc fans_pwm_counter
    andi fans_pwm_counter, 0x0f
    breq fans_pwm_handler_on
    cpse fans_pwm_counter, fans_pwm_value
    rjmp fans_pwm_handler_end
    out FANS_PORT, zero
fans_pwm_handler_end:

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
    brne adc_handler_start_again
    ; save measurement average
    mov U_raw, adc_2
    clr adc_0
    clr adc_1
    clr adc_2
    ; cannot send zero, set to 0xff if zero
    cp U_raw, zero
    brne adc_handler_start_again
    ser U_raw
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
    movw yl, miso_buffer_l
    ldi tmp1, 0x11
    st Y+, tmp1
    sbrc yh, 3
    ldi yh, 0x01
    st Y+, U_raw
    sbrc yh, 3
    ldi yh, 0x01
    movw miso_buffer_l, yl
timer_handler_end:

    ; check for incoming commands
mosi_buffer_handler:
    movw yl, mosi_buffer_l
    ld tmp0, Y
    cpse tmp0, zero
    rcall mosi_buffer_handler_has_data
    movw mosi_buffer_l, yl
mosi_buffer_handler_end:

    rjmp main_loop


mosi_buffer_handler_has_data:
    ; clear current position
    st Y, zero
    ; increase buffer pointer
    inc yl
    ; fan speed command
    cpi last_byte, 0x11
    breq mosi_buffer_handler_state
    ; fan speed command
    cpi last_byte, 0x12
    breq mosi_buffer_handler_fan_speed
    ; save value
    mov last_byte, tmp0
    ret

mosi_buffer_handler_state:
    andi tmp0, 0x3f
    mov fans, tmp0
    clr last_byte
    ret

mosi_buffer_handler_fan_speed:
    andi tmp0, 0x1f
    ; set minimum speed to 3
    ori tmp0, 0x03
    mov fans_pwm_value, tmp0
    clr last_byte
    ret


fans_pwm_handler_on:
    sbrc fans, 0
    sbi FANS_PORT, FAN0_BIT
    sbrc fans, 1
    sbi FANS_PORT, FAN1_BIT
    sbrc fans, 2
    sbi FANS_PORT, FAN2_BIT
    sbrc fans, 3
    sbi FANS_PORT, FAN3_BIT
    sbrc fans, 4
    sbi FANS_PORT, FAN4_BIT
    sbrc fans, 5
    sbi FANS_PORT, FAN5_BIT
    rjmp fans_pwm_handler_end
