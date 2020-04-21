
#include <avr/io.h>

#define __tmp_reg__     r0
#define __zero_reg__    r1

; constant registers used by interrupt routines
; these registers must not be used elsewhere
#define spi_reg_in      r25
#define spi_reg_out     r24


; spi data received interrupt
.global SPI_STC_vect
SPI_STC_vect:
    in spi_reg_in, _SFR_IO_ADDR(SPDR)
    ld spi_reg_out, Y
    out _SFR_IO_ADDR(SPDR), spi_reg_out
    ; if byte is zero, then skip incrementing Y
    cpse spi_reg_out, __zero_reg__
    ld spi_reg_out, Y+
    ; buffer is 0x0100-0x07ff, so check when Y flips to 0x0800
    sbrc yh, 3
    ldi yh, 0x01
    ; if incoming command was not zero, send to uart
    cpse spi_reg_in, __zero_reg__
    sts UDR0, spi_reg_in
    reti


.global asm_main
asm_main:
    ; initialize buffer pointers in X and Y
    ldi xh, 0x02
    ldi xl, 0x00
    ldi yh, 0x02
    ldi yl, 0x00
    st Y, __zero_reg__

    ; enable interrupts 
    sei

    ; loop here forevah!
main_loop:
    wdr ; watchdog reset

uart_rx_handler:
    ; check if new data from uart has been received
    lds r0, UCSR0A
    sbrs r0, RXC0
    rjmp next
    ; receive byte from uart
    lds r0, UDR0
    st X+, r0
    ; buffer is 0x0100-0x07ff, so check when X flips to 0x0800
    sbrc xh, 3
    ldi xh, 0x01
    ; save zero to current index so reading will know where to stop
    st X, __zero_reg__


next:
    rjmp main_loop
