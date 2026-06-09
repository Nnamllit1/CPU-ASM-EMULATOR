# 1. Welcome and Mental Model

[Manual index](README.md) | [Next: Build, Run, and Debug](02-workflow.md)

## What You Are Programming

CPU ASM contains two related environments:

- A command-line assembler and emulator for the original 16-bit CPU.
- A color console built around that CPU, with fixed hardware profiles, video, input, audio, timers, and persistent storage.

The console is not a high-level game engine. Your program writes bytes into memory and device registers. The emulator interprets those writes as pixels, tiles, sprites, button reads, tones, and saved data. This is close to how software controls simple real hardware.

## The Five Parts of a Program

Most interactive console programs can be divided into five systems:

1. **State**: positions, score, flags, and other values stored in registers or RAM.
2. **Input**: buttons currently held by the player or queued key events.
3. **Update**: rules that change state, such as movement and collision.
4. **Rendering**: writes to video memory and sprite descriptors.
5. **Timing**: deciding when an update or frame should happen.

Audio and persistent storage are additional output systems. Keeping these responsibilities separate makes assembly programs much easier to understand.

## CPU, ROM, RAM, and Devices

The **CPU** executes one 64-bit, eight-byte instruction at a time. It has 32 general-purpose 16-bit registers named `r0` through `r31`, a program counter, and a stack pointer.

The **instruction ROM** contains executable instructions and read-only data. Labels in code resolve to byte addresses in this ROM. Because an instruction occupies eight bytes, adjacent instruction labels are normally eight addresses apart.

**RAM** contains mutable program data. A byte stores 0 through 255. A word stores 0 through 65535. Arithmetic wraps to 16 bits.

**Memory-mapped devices** occupy addresses starting at `0xFF00`. Writing `3` to `0xFF10`, for example, enables the display and selects tile mode. Nothing special is required in the instruction syntax: device access uses the same load and store instructions as RAM.

## The Pocket Color Profile

The main tutorial targets Pocket Color:

| Resource | Limit |
| --- | ---: |
| CPU | 4 MHz |
| Display | 160x144 RGB332 at 30 Hz |
| RAM | 256 KiB |
| ROM | 1 MiB |
| VRAM | 64 KiB |
| Persistent storage | 64 KiB |
| Sprites | 40 total, 10 per scanline |
| Audio | 4 square-wave channels |
| Visible palette | 32 quantized colors |

The CPU can address only 16 bits at once, so larger memories are exposed through fixed-size bank windows. A bank register selects which physical portion is visible.

## Frames and Scanlines

The display is updated one horizontal row, or **scanline**, at a time. Pocket Color has 144 visible scanlines. At 30 frames per second, the hardware completes 30 full passes each emulated second.

This timing depends on emulated CPU cycles, not the speed of the host computer. Running at `0.05x` makes scanout visibly slow. Running at `10x` consumes the same emulated cycles ten times faster in real life. Unlimited mode runs as fast as the host allows and disables host audio output.

Writing to the present register at `0xFF12` restarts scanout at the top. This is useful after preparing a frame, but repeatedly presenting in a tight idle loop can keep the display trapped near its first scanline.

## First Program

This complete program prints text to the Output pane and halts:

```asm
.reset start
.org 8

start:
    %println r0 message
    hlt

message:
    .asciiz "Hello from Pocket Color."
```

`.reset start` stores the entry address in ROM bytes 0 and 1. `.org 8` leaves those bytes and the rest of the first instruction slot available, so executable code begins at address 8. `%println` expands into normal instructions that print a zero-terminated string from ROM.

## What You Will Build

The main project, Star Catcher, uses a tile background, two hardware sprites, held controls, deterministic frame timing, collision tests, a score bar, and event sounds. It is small enough to understand but already has the structure of a real game.

[Next: Build, Run, and Debug](02-workflow.md)
