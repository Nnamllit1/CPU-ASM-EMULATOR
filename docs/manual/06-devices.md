# 6. Input, Timing, Audio, and Storage

[Previous: Graphics](05-graphics.md) | [Manual index](README.md) | [Next: Build Star Catcher](07-star-catcher.md)

## Queued Input and Held Buttons

The console has two input models.

**Queued input** records discrete key events. `in` and `inkey` consume events when available. Use it for text, menus, and “press any key” behavior.

**Held buttons** expose the current console-controller state at `0xFF22-0xFF23`. Use it for continuous movement and gameplay.

| Bit | Mask | Button |
| ---: | ---: | --- |
| 0 | 1 | Up |
| 1 | 2 | Down |
| 2 | 4 | Left |
| 3 | 8 | Right |
| 4 | 16 | A |
| 5 | 32 | B |
| 6 | 64 | Start |
| 7 | 128 | Select |

Read once, then test several masks. This reduces device accesses:

```text
ldbi r0, 0xFF22
movi r1, 4
and r2, r0, r1
jnz r2, move_left
movi r1, 8
and r2, r0, r1
jnz r2, move_right
```

## Deterministic Timers

The timer macros return the low 16 bits of emulated time:

- `%millis reg`: elapsed emulated milliseconds.
- `%frames reg`: completed display frames.
- `%cycles reg`: executed CPU cycles.

All wrap naturally from 65535 to 0. Subtraction remains useful across wrap as long as the interval is shorter than 65536 units:

```text
%frames r10              ; previous frame
wait_frame:
    %frames r0
    je r0, r10, wait_frame
    mov r10, r0
    ; update once for the new frame
```

Use frames for animation tied to refresh, milliseconds for human-scale delays, and cycles for profiling or deterministic pseudo-random variation. Do not use host-speed busy loops for gameplay timing.

## Square-Wave Audio

Select a channel at `0xFF40`, configure enable, frequency, and volume at `0xFF41-0xFF44`, then let the hardware generate samples as cycles pass.

The `%tone` macro accepts frequency as low and high bytes:

```asm
.reset start
.org 8

start:
    %tone r0 0 0xB8 0x01 160   ; channel 0, 440 Hz
    %millis r10

wait:
    %millis r0
    sub r1, r0, r10
    movi r2, 500
    jlt r1, r2, wait

    %toneoff r0 0
    hlt
```

Pocket Color has four channels numbered 0 through 3. Multiple enabled channels are mixed. Keep event sounds short or replace their settings when the next event occurs.

At Unlimited desktop speed, host audio output is disabled to keep execution responsive. The emulated audio hardware still advances.

## Persistent Storage

Storage is a banked byte array visible at `E000-FEFF`. This complete program increments a counter in storage bank 0:

```asm
.reset start
.org 8

start:
    %storagebank r0 0
    ldi r1, 0xE000
    movi r2, 1
    add r1, r1, r2
    sti r1, 0xE000
    %println r0 message
    outn r1
    %newline r0
    hlt

message:
    .asciiz "Stored launch count:"
```

In the desktop Settings window, load storage before running and save storage after changes. A project’s `storage` path tells the UI which backing file to use; the assembly program still decides the binary layout.

For real saves, reserve a header such as:

```text
E000-E001  magic bytes
E002       format version
E003       flags
E004...    payload
```

Validate the magic and version before trusting the payload.

## ROM Data Banks

Use ROM banks for large read-only tables, maps, dialogue, or assets. Neon Dodge stores its tile-color table in bank 0:

```text
    movi r0, 0
    stbi r0, 0xFF02
    movi r3, tile_colors
    add r3, r3, r1
    ldbr r4, r3

.rombank 0
tile_colors:
    .byte 0x00
    .byte 0x08
    .byte 0xDA
    .byte 0xC2
```

Select the bank before every operation that depends on it, especially after calling code that may switch banks.

## Importing Image Assets

The asset tool accepts P3 or P6 PPM input and produces one RGB332 byte per pixel:

```sh
./build-console/CPU-ASM-ASSET input.ppm output.rgb332
```

The desktop can also import a PPM directly into VRAM from Settings. Imported pixels are raw graphics data; the program remains responsible for choosing the matching display mode, bank, and dimensions.

[Next: Build Star Catcher](07-star-catcher.md)
