; Full system showcase for IDKIWTEAPFF.
; Demonstrates macros, ROM data, RAM byte/word access, stack/call/return,
; ALU ops, bitwise ops, conditional jumps, output, and data directives.

%macro say reg label
    movi {reg}, {label}
    outs {reg}
%endmacro

%macro emit reg value
    movi {reg}, {value}
    out {reg}
%endmacro

start:
    %say r0 title
    %newline r0

    call print_rom_lines
    call check_alu
    call check_memory
    call check_stack
    call check_jumps

    %say r0 done_msg
    %newline r0
    hlt

print_rom_lines:
    %say r0 rom_text
    %newline r0

    ; Load a byte from ROM by immediate address and by register address.
    ldbri r1, rom_letter
    out r1
    %emit r0 32
    movi r2, rom_letter
    ldbr r3, r2
    out r3
    %newline r0

    ; Read a .word constant as two ROM bytes and rebuild the 16-bit value.
    ldbri r4, rom_word_hi
    ldbri r5, rom_word_low
    movi r6, 8
    shl r7, r4, r6
    add r7, r7, r5
    %say r0 word_text
    outn r7
    %newline r0
    ret

check_alu:
    %say r0 alu_text

    movi r1, 6
    movi r2, 7
    mul r3, r1, r2
    movi r4, 42
    je r3, r4, alu_mul_ok
    jmp fail

alu_mul_ok:
    movi r5, 5
    div r6, r3, r5
    mod r7, r3, r5
    movi r8, 8
    je r6, r8, alu_div_ok
    jmp fail

alu_div_ok:
    movi r9, 2
    je r7, r9, alu_mod_ok
    jmp fail

alu_mod_ok:
    add r10, r6, r7
    sub r11, r10, r9
    movi r12, 1
    shl r13, r11, r12
    shr r14, r13, r12
    je r14, r11, bitwise_check
    jmp fail

bitwise_check:
    movi r15, 12
    movi r16, 10
    and r17, r15, r16
    or r18, r15, r16
    xor r19, r15, r16
    not r20, r19
    movi r21, 8
    je r17, r21, bit_and_ok
    jmp fail

bit_and_ok:
    movi r21, 14
    je r18, r21, bit_or_ok
    jmp fail

bit_or_ok:
    movi r21, 6
    je r19, r21, alu_done
    jmp fail

alu_done:
    outn r3
    %newline r0
    ret

check_memory:
    %say r0 ram_text

    ; Word RAM through register address.
    movi r1, 4096
    movi r2, 1234
    st r1, r2
    ld r3, r1
    je r3, r2, word_ram_ok
    jmp fail

word_ram_ok:
    ; Word RAM through immediate address.
    movi r4, 4321
    sti r4, 4100
    ldi r5, 4100
    je r5, r4, byte_ram_ok
    jmp fail

byte_ram_ok:
    ; Byte RAM through immediate and register addresses.
    movi r6, 65
    stbi r6, 4200
    ldbi r7, 4200
    je r7, r6, byte_reg_ok
    jmp fail

byte_reg_ok:
    movi r8, 4201
    movi r9, 90
    stb r8, r9
    ldb r10, r8
    je r10, r9, memory_done
    jmp fail

memory_done:
    out r7
    out r10
    %newline r0
    ret

check_stack:
    %say r0 stack_text

    movi r1, 79
    movi r2, 75
    push r1
    push r2
    pop r3
    pop r4
    out r4
    out r3
    %newline r0

    ; mov copies, movc copies and clears the source.
    mov r5, r4
    movc r6, r5
    jz r5, movc_ok
    jmp fail

movc_ok:
    jnz r6, stack_done
    jmp fail

stack_done:
    ret

check_jumps:
    %say r0 jump_text

    movi r1, 2
    movi r2, 5
    jlt r1, r2, less_ok
    jmp fail

less_ok:
    jle r1, r2, le_ok
    jmp fail

le_ok:
    jgt r2, r1, gt_ok
    jmp fail

gt_ok:
    jge r2, r1, ne_ok
    jmp fail

ne_ok:
    jne r1, r2, jump_done
    jmp fail

jump_done:
    %say r0 ok_msg
    %newline r0
    ret

fail:
    %newline r0
    %say r0 fail_msg
    %newline r0
    hlt

title:
    .ascii "== FULL "
    .asciiz "SYSTEM SHOWCASE =="

rom_text:
    .asciiz "ROM bytes:"
rom_letter:
    .byte 82

word_text:
    .asciiz "ROM word: "

rom_word_hi:
    .byte 7
rom_word_low:
    .byte 234
rom_word_mirror:
    .word 2026

alu_text:
    .asciiz "ALU result: "

ram_text:
    .asciiz "RAM bytes: "

stack_text:
    .asciiz "Stack says: "

jump_text:
    .asciiz "Jump checks: "

ok_msg:
    .asciiz "OK"

done_msg:
    .asciiz "ALL CHECKS PASSED"

fail_msg:
    .asciiz "CHECK FAILED"
