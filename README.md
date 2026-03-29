# CPU-ASM-EMULATOR

This is a simple emulator and ASM compiler for the "IDK i want to emulate a processor for fun" processor (or IDKIWTEAPFF for short).

## Specifications

- CPU: 16 bit
  - Registers: 32 registers
  - Instruction set: IDK yet :D
- Memory: 64 KiB (or more? Maybe?)
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
    add r0, r2
    sub r0, r3
    jmp label
```

#### Instruction format

| Opcode | Rx | Ry | Undefined (or Special) |
| --- | --- | --- | --- |
| xxxxxxxxxxxxxxxx | xxxxxxxxxxxxxxxx | xxxxxxxxxxxxxxxx | xxxxxxxxxxxxxxxx |

#### Basic instructions

| Instruction | Usage | Description | Opcode |
| --- | --- | --- | --- |
| movi | rX, imm | Move immediate to register | 0000000000000000 |
| mov | rX, rY | Move register to register, but dosent clear the source register (just like a copy) | 0000000000000001 |
| movc | rX, rY | Move register to register, clearing the source register | 0000000000000010 |
| add | rX, rY | Add register to register | 0000000000000011 |
| sub | rX, rY | Subtract register from register | 0000000000000100 |
| srl | rX, rY | Shift register left logically (0000000001010110 -> 0000000010101100 so 86 -> 172) | 0000000000000101 |
| srr | rX, rY | Shift register right logically (0000000010101100 -> 0000000001010110 so 172 -> 86) | 0000000000000110 |
| jmp | label | Jump to label | 0000000000000111 |

More instructions will be added later.
