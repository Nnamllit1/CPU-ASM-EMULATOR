# 4. The Console Hardware

[Previous: Assembly Foundations](03-assembly-foundations.md) | [Manual index](README.md) | [Next: Graphics](05-graphics.md)

## Address Map

The CPU uses a 16-bit data address, so it can see 65536 addresses at once:

| Address | Purpose |
| --- | --- |
| `0000-7FFF` | Fixed RAM |
| `8000-BFFF` | 16 KiB banked RAM window |
| `C000-DFFF` | 8 KiB banked VRAM window |
| `E000-FEFF` | 7936-byte banked persistent-storage window |
| `FF00-FFFF` | Device registers |

Instruction ROM is separate from data memory. Its lower `0000-7FFF` range is fixed, while `8000-FFFF` is a switchable 32 KiB ROM bank.

## Memory-Mapped I/O

Device registers are ordinary addresses with special behavior. This enables tile mode:

```text
movi r0, 3
stbi r0, 0xFF10
```

Bit 0 enables display output. Bit 1 selects tile mode. Therefore:

- `0`: display disabled.
- `1`: display enabled in bitmap mode.
- `3`: display enabled in tile mode.

Device accesses cost four cycles, compared with two cycles for normal memory operations.

## Profile Detection

`0xFF30` reports `0` for Pocket Color, `1` for Home 16, and `2` for Custom Hardware. A program that relies on exact dimensions should fail clearly on the wrong profile:

```asm
.reset start
.org 8

start:
    ldbi r0, 0xFF30
    jz r0, pocket_ok
    %println r0 wrong_profile
    hlt

pocket_ok:
    %println r0 ready
    hlt

wrong_profile:
    .asciiz "This example requires Pocket Color."
ready:
    .asciiz "Pocket Color detected."
```

Do this near startup for examples with hard-coded `160x144` or `20x18` layouts.

## RAM Banks

Writing to `0xFF00` selects the physical RAM visible at `8000-BFFF`:

```text
%rambank r0 1
movi r1, 0x42
stbi r1, 0x8000

%rambank r0 2
movi r1, 0x99
stbi r1, 0x8000
```

The two writes use the same CPU address but reach different physical bytes. Fixed RAM remains visible while banks change, so store the active bank number or shared control state there.

## VRAM Banks

`0xFF01` selects the 8 KiB VRAM window at `C000-DFFF`. Physical VRAM offsets map as follows:

```text
physical offset = bank * 8192 + (CPU address - 0xC000)
```

Pocket Color's bitmap requires 23040 bytes, so it spans banks 0, 1, and 2. Tile graphics occupy physical offsets `0000-3FFF`, the tile map begins at `4000`, and sprites begin at `5000`. Both the tile map and sprite table therefore appear through VRAM bank 2.

## ROM Banks

Place switchable ROM data with `.rombank N`. Labels inside each bank use logical addresses starting at `0x8000`. Select the same bank number through `0xFF02` before reading:

```text
    %rombank r0 0
    ldbri r1, item_cost

.rombank 0
item_cost:
    .byte 25
```

`.rombank` values must increase and may range from 0 through 254. Code normally stays in fixed ROM so changing a data bank does not change the instructions being executed.

## Persistent Storage Banks

`0xFF03` selects storage visible through `E000-FEFF`. Storage survives only when the host loads and saves its backing file. The program should use a header, version, and checksum if incompatible or partial data would be dangerous.

## Hardware Faults

The machine can stop with deterministic faults for an invalid register, division by zero, unknown opcode, oversized ROM, invalid profile, program counter outside ROM, or excessive sprite count. The current code is exposed at `0xFF31` and described in the debugger.

Treat a fault as a program bug or incompatible project profile. Do not hide it with an idle loop.

## Hardware Profiles

Pocket Color and Home 16 are fixed reference consoles. Custom Hardware starts from a reference preset and allows individual limits to change. Build against Pocket Color first when possible: a program that behaves under the smallest limits is easier to scale upward.

Changing display dimensions affects bitmap size and tile-map width. Changing refresh rate affects frame timing. Changing clock speed affects how much CPU work fits in one frame. These fields are related; Custom Hardware is not only a performance switch.

[Next: Graphics](05-graphics.md)
