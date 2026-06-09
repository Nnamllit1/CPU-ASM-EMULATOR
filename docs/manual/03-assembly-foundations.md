# 3. Assembly Foundations

[Previous: Build, Run, and Debug](02-workflow.md) | [Manual index](README.md) | [Next: The Console Hardware](04-console-hardware.md)

## Registers and Values

Registers are small, fast storage locations inside the CPU. Every general-purpose register is 16 bits wide. The instruction below puts decimal `42` in `r0`:

```text
movi r0, 42
```

Numbers may be decimal, hexadecimal, binary, or a character literal:

```text
movi r0, 160
movi r1, 0x00A0
movi r2, 0b10100000
movi r3, 'A'
```

These four forms are useful for different reasons: decimal for counts, hexadecimal for addresses and colors, binary for bit masks, and characters for text.

## Arithmetic and Assignment

Instructions name the destination first:

```text
add r0, r1, r2     ; r0 = r1 + r2
sub r3, r3, r4     ; r3 = r3 - r4
mul r5, r6, r7
div r8, r9, r10
mod r11, r12, r13
```

Arithmetic wraps at 65535. There are no negative-number-specific branch instructions; comparisons treat register values as unsigned.

`mov` copies a register. `movc` copies it and clears the source:

```text
mov  r1, r0        ; both contain the value
movc r2, r1        ; r2 receives it, r1 becomes zero
```

## Labels, Jumps, and Loops

A label names a ROM address. Jumps replace the program counter with that address.

This complete program prints the numbers 0 through 9:

```asm
.reset start
.org 8

start:
    movi r0, 0
    movi r1, 10
    movi r2, 1

loop:
    outn r0
    %space r3
    add r0, r0, r2
    jlt r0, r1, loop

    %newline r3
    hlt
```

Conditional jumps compare one or two registers. `jz` and `jnz` test zero. `je`, `jne`, `jlt`, `jle`, `jgt`, and `jge` compare two unsigned values.

## Bits and Button Masks

Bitwise instructions operate on individual bits:

```text
and r0, r1, r2
or  r0, r1, r2
xor r0, r1, r2
not r0, r1
shl r0, r1, r2
shr r0, r1, r2
```

Hardware often packs several booleans into one number. If the Left button is bit 2, its mask is `4`. To test it:

```text
ldbi r0, 0xFF22
movi r1, 4
and r2, r0, r1
jnz r2, left_is_held
```

## RAM: Bytes and Words

Use byte operations for colors, tile numbers, button masks, and values known to stay below 256. Use word operations for timers, scores, addresses, and larger counters.

```text
movi r0, 200
stbi r0, 0x0100    ; write low byte
ldbi r1, 0x0100    ; read byte

movi r2, 5000
sti r2, 0x0102     ; write 16-bit word
ldi r3, 0x0102     ; read word
```

Words are stored big-endian: the high byte is at the named address and the low byte is at the following address. Do not overlap unrelated values with either byte.

Register-address forms are useful in loops:

```text
movi r0, 0x0200    ; address
movi r1, 7         ; value
stb r0, r1
ldb r2, r0
```

## Subroutines and the Stack

`call label` pushes a return address and jumps. `ret` pops that address and returns. A subroutine should document which registers it expects and changes.

```asm
.reset start
.org 8

start:
    movi r0, 12
    movi r1, 30
    call add_pair
    outn r0
    %newline r2
    hlt

; Input: r0, r1
; Output: r0
add_pair:
    add r0, r0, r1
    ret
```

Use `push` and `pop` when a subroutine must preserve a caller-owned register:

```text
draw_object:
    push r5
    ; use r5 temporarily
    pop r5
    ret
```

Every push needs a matching pop on every path. Unbalanced stack use eventually corrupts return addresses.

## ROM Data and Directives

Data directives emit bytes into instruction ROM:

```text
title:
    .asciiz "Pocket program"
values:
    .byte 1
    .byte 2
    .word 1000
```

Read ROM bytes with `ldbr`/`ldbri` and words with `ldwr`/`ldwri`. ROM data is read-only. `.space`, `.align`, and `.org` move or reserve output positions; they do not allocate RAM.

## Macros

Macros are assembler-time text expansions. Built-ins include `%println`, `%vbank`, `%present`, `%tone`, and the timer helpers. They are conveniences, not hidden runtime services.

You can define a macro with placeholders:

```text
%macro clear_byte address temp
    movi {temp}, 0
    stbi {temp}, {address}
%endmacro
```

Use macros for short repeated register sequences. Use subroutines for runtime behavior, especially when code size matters.

## Suggested Practice

Before continuing, modify the counting program to:

1. Count by two.
2. Store the current value in RAM before printing it.
3. Put printing in a subroutine.
4. Stop when the value is greater than or equal to 20.

[Next: The Console Hardware](04-console-hardware.md)
