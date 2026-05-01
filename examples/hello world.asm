; Hello World for IDKIWTEAPFF
; The putc macro expands to the two instructions needed to print one ASCII char.

%macro putc reg value
    movi {reg}, {value}
    out {reg}
%endmacro

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
    %putc r0 10

    hlt
