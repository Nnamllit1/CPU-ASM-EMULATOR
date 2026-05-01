# CPU-ASM-EMULATOR

This is a simple emulator and ASM compiler for the "IDK i want to emulate a processor for fun" processor (or IDKIWTEAPFF for short).

## Specifications

- CPU: 16 bit
  - Registers: 32 registers
  - Register fields in instructions are 8 bits wide
  - Instruction set: IDK yet :D
- Memory: 64 KiB (Implemented now :D)
- Instruction set:

## ASM Instructions

### Registers

| Register | Description |
| --- | --- |
| r0 | General purpose register |
| r1 | General purpose register |
| r2 | General purpose register |
| r3 | General purpose register |
| r4 | General purpose register |
| r5 | General purpose register |
| r6 | General purpose register |
| r7 | General purpose register |
| r8 | General purpose register |
| r9 | General purpose register |
| r10 | General purpose register |
| r11 | General purpose register |
| r12 | General purpose register |
| r13 | General purpose register |
| r14 | General purpose register |
| r15 | General purpose register |
| r16 | General purpose register |
| r17 | General purpose register |
| r18 | General purpose register |
| r19 | General purpose register |
| r20 | General purpose register |
| r21 | General purpose register |
| r22 | General purpose register |
| r23 | General purpose register |
| r24 | General purpose register |
| r25 | General purpose register |
| r26 | General purpose register |
| r27 | General purpose register |
| r28 | General purpose register |
| r29 | General purpose register |
| r30 | General purpose register |
| r31 | General purpose register |

### Instructions

Instruction names are not case sensitive.
Instructions are 64 bits long.

#### Labels

Labels are used to mark a position in the code.
Labels are used to jump to a position in the code.

##### Example

``` asm
label:
    mov r0, r1
    add r0, r0, r2
    sub r0, r0, r3
    jmp label
```

#### Instruction format

| Opcode | Rx | Ry | Rz | Mode | Special / Immediate / Label |
| --- | --- | --- | --- | --- | --- |
| xxxxxxxxxxxxxxxx | xxxxxxxx | xxxxxxxx | xxxxxxxx | xxxxxxxx | xxxxxxxxxxxxxxxx |

#### Basic instructions

| Instruction | Usage | Description | Opcode |
| --- | --- | --- | --- |
| movi | rX, imm | Move immediate to register | 0000000000000000 |
| mov | rX, rY | Move register to register, but dosent clear the source register (just like a copy) | 0000000000000001 |
| movc | rX, rY | Move register to register, clearing the source register | 0000000000000010 |
| add | rX, rY, rZ | Add registers (rX = rY + rZ) | 0000000000000011 |
| sub | rX, rY, rZ | Subtract registers (rX = rY - rZ) | 0000000000000100 |
| shl | rX, rY, rZ | Shift register left logically (rX = rY << rZ) | 0000000000000101 |
| shr | rX, rY, rZ | Shift register right logically (rX = rY >> rZ) | 0000000000000110 |
| jmp | label | Jump to label | 0000000000000111 |
| jz | rX, label | Jump to label if zero | 0000000000001000 |
| jnz | rX, label | Jump to label if not zero | 0000000000001001 |
| je | rX, rY, label | Jump to label if equal | 0000000000001010 |
| jne | rX, rY, label | Jump to label if not equal | 0000000000001011 |
| hlt | | Halt the CPU | 0000000000001100 |
| out | rX | push contents of the register to stdout (as ascii character from decimal value, a = 97 and z = 122) | 0000000000001101 |
| ld | rX, rY | Load register from memory (rX = memory[rY]) | 0000000000001110 |
| ldi | rX, imm | Load immediate from memory (rX = memory[imm]) | 0000000000001111 |
| st | rX, rY | Store register to memory (memory[rX] = rY) | 0000000000010000 |
| sti | rX, imm | Store immediate to memory (memory[imm] = rX) | 0000000000010001 |
| mul | rX, rY, rZ | Multiply registers (rX = rY * rZ) | 0000000000010010 |
| div | rX, rY, rZ | Divide registers (rX = rY / rZ) | 0000000000010011 |
| mod | rX, rY, rZ | Modulo registers (rX = rY % rZ) | 0000000000010100 |

More instructions will be added later.

### Macros

Macros are used to simplify the assembly code.

More instructions will be added later.

## Assembler

The assembler is a simple text-based assembler.

### Syntax

#### Comments

Comments are denoted by a `;` at the beginning of the line.
Comments can be placed anywhere in the line.

##### Example

``` asm
; This is a comment
```

#### Labels

Labels are denoted by a `:` at the end of the line.
Labels can be placed anywhere in the line.

##### Example

``` asm
start:
```

#### Macros

Macros are denoted by a `%` at the beginning of the line.
Macros can be placed anywhere in the line.
Default macros are compiled into the emulator executable and are available in every assembled file.

##### Example

``` asm
%putc r0 97
%newline r0
hlt
```
