; Hello World for IDKIWTEAPFF
; Uses default macros compiled into the emulator executable.

start:
    ; "Hello World\n"
    %putc r0 72
    %putc r0 101
    %putc r0 108
    %putc r0 108
    %putc r0 111
    %putc r0 32
    %putc r0 87
    %putc r0 111
    %putc r0 114
    %putc r0 108
    %putc r0 100
    %newline r0

    hlt
