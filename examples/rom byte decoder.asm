; ROM byte loading demo for IDKIWTEAPFF.
; The message bytes below are stored encrypted in instruction ROM.
; The program reads them with ldbr/ldbri, decodes them, and prints the result.

start:
    movi r0, banner
    outs r0
    %newline r0

    ; Load the XOR key directly from instruction ROM.
    ldbri r4, xor_key

    ; r2 walks through the encrypted ROM byte range.
    ; r3 points one byte past the encrypted message.
    movi r2, encrypted_message
    movi r3, encrypted_end
    movi r8, 1

decode_loop:
    ldbr r5, r2
    xor r6, r5, r4
    out r6
    add r2, r2, r8
    jlt r2, r3, decode_loop

    %newline r0
    movi r0, status
    outs r0
    %newline r0

    movi r0, key_text
    outs r0
    outn r4
    %newline r0

    hlt

banner:
    .asciiz "== ROM BYTE DECODER =="

status:
    .asciiz "decoded from instruction ROM"

key_text:
    .asciiz "xor key: "

xor_key:
    .byte 42

encrypted_message:
    ; "THE ROM KNOWS!" XOR 42
    .byte 126
    .byte 98
    .byte 111
    .byte 10
    .byte 120
    .byte 101
    .byte 103
    .byte 10
    .byte 97
    .byte 100
    .byte 101
    .byte 125
    .byte 121
    .byte 11
encrypted_end:
