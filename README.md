# CPU-ASM-EMULATOR

This is a simple emulator and ASM compiler for the "IDK i want to emulate a processor for fun" processor (or IDKIWTEAPFF for short).

## Specifications

- CPU: IDK (mabey 64-bit? With other modes?)
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

#### Basic instructions

| Instruction | Usage | Description |
| --- | --- | --- |
| movi | rX, imm | Move immediate to register |
| movr | rX, rY | Move register to register, but dosent clear the source register (just like a copy) |
| mov | rX, rY | Move register to register, clearing the source register |
| add | rX, rY | Add register to register |
| sub | rX, rY | Subtract register from register |
| srl | rX, rY | Shift register left logically (01010110 -> 10101100 so 86 -> 172) |
| srr | rX, rY | Shift register right logically (10101100 -> 01010110 so 172 -> 86) |

More instructions will be added later.
