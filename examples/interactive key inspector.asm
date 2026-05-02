; Interactive key inspector for IDKIWTEAPFF.
; Press keys to see what the program receives.
; Press q or Q to quit.

start:
    %print r0 title
    %newline r0
    %print r0 instructions
    %newline r0
    %print r0 prompt

wait_key:
    ; `in` is non-blocking and leaves the register unchanged when no key exists.
    ; Clear r1 first so zero means "nothing read yet" for this polling pass.
    movi r1, 0
    in r1
    jz r1, wait_key

    movi r2, 'q'
    je r1, r2, quit
    movi r2, 'Q'
    je r1, r2, quit

    %print r0 seen_text
    out r1
    %print r0 code_text
    outn r1
    %newline r0
    %print r0 prompt
    jmp wait_key

quit:
    %newline r0
    %print r0 bye_text
    %newline r0
    hlt

title:
    .asciiz "== KEY INSPECTOR =="

instructions:
    .asciiz "Press any key. Press q to quit."

prompt:
    .asciiz "> "

seen_text:
    .asciiz "key='"

code_text:
    .asciiz "' code="

bye_text:
    .asciiz "bye"
