# CPU ASM Console Complete Summary

This document is a self-contained specification for writing assembly programs for the CPU ASM experimental console. It describes the current implementation, including behavior that may be surprising. When this document conflicts with general assembly conventions, follow this document.

## 1. Recommended Program Shape

Use this startup pattern for console programs:

```asm
.reset start
.org 8

start:
    ; Optional exact-profile check.
    ldbi r0, 0xFF30
    jz r0, pocket_ok
    %println r0 wrong_profile
    hlt

pocket_ok:
    ; Initialize RAM and VRAM, enable the PPU, then enter the main loop.
main_loop:
    jmp main_loop

wrong_profile:
    .asciiz "This program requires Pocket Color."
```

`.reset start` writes a big-endian reset address into physical ROM bytes 0 and 1 and also sets the tool entry point. `.org 8` prevents the first eight-byte instruction from being partly replaced by the reset vector.

For an interactive program, organize work as:

```text
wait for timer -> read input -> update state -> resolve rules -> update graphics/audio -> present -> repeat
```

## 2. CPU Model

- 32 general-purpose registers: `r0` through `r31`.
- Registers are unsigned 16-bit values. Arithmetic wraps modulo 65536.
- The program counter and stack pointer are 16-bit.
- Each instruction is exactly eight ROM bytes.
- Adjacent instructions normally have addresses eight bytes apart.
- Instruction mnemonics and register names are case-insensitive.
- Labels and macro parameter names are case-sensitive.
- Comparisons are unsigned.
- There are no flags, signed arithmetic instructions, interrupts, floating point, indirect jumps, or dynamic allocation.
- The initial stack pointer is `0xFFFE`. The stack grows downward by two bytes.
- Stack operations access RAM backing directly, even though the stack address overlaps the device page. Ordinary `ld`/`st` at `0xFF00+` still access devices.

### Instruction Encoding

Every instruction is encoded big-endian as:

| Bits | Field |
| --- | --- |
| 63-48 | 16-bit opcode |
| 47-40 | `rx` register index |
| 39-32 | `ry` register index |
| 31-24 | `rz` register index |
| 23-16 | mode, currently zero |
| 15-0 | immediate or label address |

The program counter advances by eight unless an instruction branches, calls, returns, halts, or faults.

## 3. Complete Instruction Set

Cycle costs are deterministic. ALU operations normally cost one cycle, a taken branch costs two, RAM/VRAM/storage access costs two, device access costs four, and stack/call operations cost three.

### Data Movement

| Opcode | Syntax | Semantics | Cycles |
| ---: | --- | --- | ---: |
| `0000` | `movi rX, value` | `rX = value` | 1 |
| `0001` | `mov rX, rY` | `rX = rY`; source unchanged | 1 |
| `0002` | `movc rX, rY` | `rX = rY`; then `rY = 0` | 1 |

Immediates may be numeric literals or labels. A negative immediate is converted to its 16-bit two's-complement bit pattern.

### Arithmetic

| Opcode | Syntax | Semantics | Cycles |
| ---: | --- | --- | ---: |
| `0003` | `add rX, rY, rZ` | `rX = rY + rZ` | 1 |
| `0004` | `sub rX, rY, rZ` | `rX = rY - rZ` | 1 |
| `0012` | `mul rX, rY, rZ` | low 16 bits of `rY * rZ` | 1 |
| `0013` | `div rX, rY, rZ` | unsigned `rY / rZ` | 1 or fault |
| `0014` | `mod rX, rY, rZ` | unsigned `rY % rZ` | 1 or fault |

`div` and `mod` fault when `rZ` is zero.

### Shifts and Bitwise Operations

| Opcode | Syntax | Semantics | Cycles |
| ---: | --- | --- | ---: |
| `0005` | `shl rX, rY, rZ` | logical left shift | 1 |
| `0006` | `shr rX, rY, rZ` | logical right shift | 1 |
| `0015` | `and rX, rY, rZ` | bitwise AND | 1 |
| `0016` | `or rX, rY, rZ` | bitwise OR | 1 |
| `0017` | `xor rX, rY, rZ` | bitwise XOR | 1 |
| `0018` | `not rX, rY` | 16-bit bitwise NOT | 1 |

If a shift count is 16 or greater, the result is zero.

### Branches

| Opcode | Syntax | Taken condition | Cycles |
| ---: | --- | --- | ---: |
| `0007` | `jmp label` | Always | 2 |
| `0008` | `jz rX, label` | `rX == 0` | 2 taken, 1 otherwise |
| `0009` | `jnz rX, label` | `rX != 0` | 2 taken, 1 otherwise |
| `000A` | `je rX, rY, label` | `rX == rY` | 2 taken, 1 otherwise |
| `000B` | `jne rX, rY, label` | `rX != rY` | 2 taken, 1 otherwise |
| `0019` | `jlt rX, rY, label` | unsigned `rX < rY` | 2 taken, 1 otherwise |
| `001A` | `jle rX, rY, label` | unsigned `rX <= rY` | 2 taken, 1 otherwise |
| `001B` | `jgt rX, rY, label` | unsigned `rX > rY` | 2 taken, 1 otherwise |
| `001C` | `jge rX, rY, label` | unsigned `rX >= rY` | 2 taken, 1 otherwise |

Branch targets must be labels. There is no branch-to-register instruction.

### RAM and Device Memory

| Opcode | Syntax | Semantics |
| ---: | --- | --- |
| `000E` | `ld rX, rY` | Read big-endian word at address in `rY` |
| `000F` | `ldi rX, address` | Read big-endian word at immediate address |
| `0010` | `st rX, rY` | Write `rY` as big-endian word at address in `rX` |
| `0011` | `sti rX, address` | Write `rX` as big-endian word at immediate address |
| `0023` | `ldb rX, rY` | Read byte at address in `rY`, zero-extended |
| `0024` | `stb rX, rY` | Write low byte of `rY` at address in `rX` |
| `0025` | `ldbi rX, address` | Read byte at immediate address |
| `0026` | `stbi rX, address` | Write low byte of `rX` at immediate address |

Memory operations cost two cycles except addresses `0xFF00-0xFFFF`, which cost four. Word reads/writes access the named address first and the next 16-bit address second. Address `0xFFFF + 1` wraps to `0x0000`.

### Instruction-ROM Data Reads

| Opcode | Syntax | Semantics | Cycles |
| ---: | --- | --- | ---: |
| `0027` | `ldbr rX, rY` | Read ROM byte at address in `rY` | 2 |
| `0028` | `ldbri rX, address` | Read ROM byte at immediate address | 2 |
| `0029` | `ldwr rX, rY` | Read big-endian ROM word at address in `rY` | 2 |
| `002A` | `ldwri rX, address` | Read big-endian ROM word at immediate address | 2 |

ROM reads at logical address `0x8000+` use the currently selected ROM bank. Out-of-image reads return zero.

### Text and Queued Input

| Opcode | Syntax | Semantics | Cycles |
| ---: | --- | --- | ---: |
| `000D` | `out rX` | Append ASCII character when `rX <= 127` | 4 |
| `0021` | `outn rX` | Append unsigned decimal value | 4 |
| `0022` | `outs rX` | Print zero-terminated ROM string at `rX` | 4 |
| `002B` | `in rX` | Consume one queued input value if available | 4 |
| `002C` | `inkey rX` | Consume one queued raw-key value if available | 4 |

If no queued input exists, `in` and `inkey` leave the destination unchanged. In the console core their execution is identical; the desktop decides what values it queues. `outs` follows the current ROM bank for addresses at or above `0x8000`.

### Stack and Control

| Opcode | Syntax | Semantics | Cycles |
| ---: | --- | --- | ---: |
| `000C` | `hlt` | Enter Halted state and append a halt message to Output | 1 |
| `001D` | `push rX` | `SP -= 2`; store `rX` big-endian | 3 |
| `001E` | `pop rX` | load word at `SP`; `SP += 2` | 3 |
| `001F` | `call label` | Push `PC + 8`; branch to label | 3 |
| `0020` | `ret` | Pop PC | 3 |

`run` cannot resume a Halted or Faulted machine. Reset or reload it first.

## 4. Assembler Syntax and Quirks

### Lines and Comments

- Whitespace is flexible.
- Commas are optional separators because commas are converted to spaces.
- `;` begins a comment anywhere on the line.
- Important quirk: comments are removed before string parsing, so a semicolon cannot safely appear inside `.ascii` or `.asciiz` text.
- A label must be alone on a line and end with `:`.
- Labels are case-sensitive and duplicates are rejected.
- Instructions and registers are case-insensitive.
- Keep directives lowercase. First-pass directive recognition is lowercase-sensitive.

### Number Literals

Accepted forms:

```text
123          decimal
-1           signed spelling, encoded as 0xFFFF when used as an immediate
0x7B         hexadecimal
0b1111011    binary
'A'          character
'\n'         escaped character
```

Character escapes are `\0`, `\n`, `\r`, `\t`, `\\`, and `\'`. Numeric magnitude must not exceed 65535. `.byte` rejects values outside 0-255; data directives generally reject negative values even though instruction immediates accept them.

### Directives

| Syntax | Exact behavior |
| --- | --- |
| `.byte value` | Emit exactly one byte; value 0-255 |
| `.word value` | Emit one big-endian word; value 0-65535 |
| `.ascii "text"` | Emit bytes between first and last quote, no terminator, no escape processing |
| `.asciiz "text"` | Same, followed by zero |
| `.space count` | Emit `count` zero bytes |
| `.align N` | Emit zeros until the logical ROM address is divisible by `N` |
| `.org address` | Emit zeros until the logical address reaches `address`; cannot move backward |
| `.entry label` | Set tool entry point; emits no bytes |
| `.reset label` | Set entry point and patch physical ROM bytes 0-1 big-endian |
| `.rombank N` | Begin switchable ROM bank `N`; emits padding to physical `(N+1)*0x8000` and sets logical address to `0x8000` |

Fixed ROM content must fit logical `0x0000-0x7FFF`. Every switchable bank must fit logical `0x8000-0xFFFF`. `.rombank` numbers must strictly increase from 0 through 254. Skipped bank numbers create zero-filled physical gaps.

Do not put executable code that must remain reachable after switching banks in the switchable ROM region. Keep core code in fixed ROM and use banks primarily for read-only data.

### Macros

Definition and invocation:

```text
%macro name parameter
    movi {parameter}, 1
%endmacro

%name r0
```

- Macro names are normalized to lowercase.
- Definitions must appear before use.
- Nested definitions are rejected.
- Expansion is a single pass; do not rely on a macro body invoking another macro.
- Arguments are whitespace tokens; quoted strings with spaces are not suitable macro arguments.
- Expansion uses literal replacement of `{parameter}`.
- Macro-expanded instructions map back to the invocation line in the debugger.
- The CLI option `--nodefaults` disables all built-in macros.

### Built-In Macros

| Macro | Expansion purpose |
| --- | --- |
| `%putc reg value` | Load and print one character |
| `%putn reg value` | Load and print one number |
| `%space reg` | Print ASCII space |
| `%newline reg` | Print line feed |
| `%print reg label` | Load label and print zero-terminated ROM string |
| `%println reg label` | Print string then line feed |
| `%read reg` | `in reg` |
| `%readkey reg` | `inkey reg` |
| `%rambank reg bank` | Write RAM bank register |
| `%vbank reg bank` | Write VRAM bank register |
| `%rombank reg bank` | Write ROM bank register |
| `%storagebank reg bank` | Write storage bank register |
| `%ppumode reg mode` | Write PPU control |
| `%present reg` | Write present register |
| `%sprites reg count` | Write only low sprite-count byte `0xFF13` |
| `%tone reg channel lo hi volume` | Select, configure, and enable an audio channel |
| `%toneoff reg channel` | Select and disable an audio channel |
| `%millis reg` | Read big-endian word at `0xFF50` |
| `%frames reg` | Read big-endian word at `0xFF52` |
| `%cycles reg` | Read big-endian word at `0xFF54` |

`%sprites` does not clear `0xFF14`; clear the high byte explicitly when necessary.

## 5. Hardware Profiles

| Profile | ID | CPU | Display | RAM | ROM | VRAM | Storage | Sprites | Palette | Audio |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- | ---: | ---: |
| Pocket Color | 0 | 4 MHz | 160x144 @ 30 Hz | 256 KiB | 1 MiB | 64 KiB | 64 KiB | 40, 10/line | 32 | 4 |
| Home 16 | 1 | 8 MHz | 320x240 @ 50 Hz | 1 MiB | 4 MiB | 256 KiB | 1 MiB | 128, 32/line | 256 | 8 |
| Custom Hardware | 2 | configurable | configurable | configurable | configurable | configurable | configurable | configurable | 2-256 | 0-64 |

Custom validation limits:

- Clock: 1 Hz to 1 GHz.
- Display: each dimension 64 to 3840/2160; desktop sliders currently expose up to 1920x1080.
- Refresh: 1-240 Hz.
- RAM: 64 KiB to 256 MiB.
- ROM: 64 KiB to 512 MiB.
- VRAM: at least `width * height` bytes and at most 256 MiB.
- Storage: 0-256 MiB.
- Sprites per scanline must not exceed total sprites.
- Palette: 2-256 colors.
- Audio channels: 0-64.

Custom Hardware reports profile ID 2 even when copied from Pocket Color or Home 16. Programs requiring exact Pocket dimensions should test `0xFF30 == 0`.

## 6. Data Address Map and Banking

| CPU address | Purpose | Bank window |
| --- | --- | ---: |
| `0000-7FFF` | Fixed RAM | none |
| `8000-BFFF` | Banked RAM | 16384 bytes |
| `C000-DFFF` | Banked VRAM | 8192 bytes |
| `E000-FEFF` | Banked persistent storage | 7936 bytes |
| `FF00-FFFF` | Device registers | none |

Bank registers are bytes. Selected bank numbers wrap modulo the number of available banks. Therefore an out-of-range bank usually aliases an existing bank instead of faulting.

RAM physical layout reserves physical `0x0000-0x7FFF` for fixed RAM; banked RAM starts at physical `0x8000`. VRAM and storage begin their bank calculations at physical offset zero.

Storage may be configured as zero bytes. Reads then return zero and writes do nothing.

### Instruction ROM

- Logical `0x0000-0x7FFF`: physical fixed bank at image offset 0.
- Logical `0x8000-0xFFFF`: selected switchable bank.
- `.rombank N` data begins at physical image offset `(N + 1) * 0x8000`.
- Writing `N` to `0xFF02` selects emitted bank `N` when that bank exists.
- ROM bank wrapping uses the number of banks present in the loaded image, not the profile's full ROM allocation.
- Missing/out-of-image ROM bytes read as zero.

## 7. Device Registers

| Address | Read/write behavior |
| --- | --- |
| `FF00` | RAM bank selector |
| `FF01` | VRAM bank selector |
| `FF02` | ROM bank selector |
| `FF03` | Storage bank selector |
| `FF10` | PPU control: bit 0 display enable, bit 1 tile mode |
| `FF11` | PPU status: 1 only while current scanline index is zero |
| `FF12` | Any write restarts scanout at line zero and immediately renders line zero |
| `FF13` | Sprite count low byte |
| `FF14` | Sprite count high byte |
| `FF20` | Queued input status: 1 when queue is nonempty |
| `FF21` | Read front queued byte without consuming; any write consumes one queued value |
| `FF22` | Held-button mask low byte |
| `FF23` | Held-button mask high byte |
| `FF30` | Profile ID: 0 Pocket, 1 Home, 2 Custom |
| `FF31` | Fault code |
| `FF40` | Selected audio channel |
| `FF41` | Selected channel enable in bit 0 |
| `FF42` | Selected channel frequency low byte |
| `FF43` | Selected channel frequency high byte |
| `FF44` | Selected channel volume 0-255 |
| `FF50-FF51` | Big-endian low 16 bits of emulated milliseconds |
| `FF52-FF53` | Big-endian low 16 bits of completed frames |
| `FF54-FF55` | Big-endian low 16 bits of CPU cycles |

Unknown device-register reads return zero; unknown writes are ignored.

### Held Buttons

| Bit | Mask | Button | Default keyboard |
| ---: | ---: | --- | --- |
| 0 | 1 | Up | Up arrow |
| 1 | 2 | Down | Down arrow |
| 2 | 4 | Left | Left arrow |
| 3 | 8 | Right | Right arrow |
| 4 | 16 | A | Z |
| 5 | 32 | B | X |
| 6 | 64 | Start | Enter |
| 7 | 128 | Select | Right Shift |

Read `0xFF22` once per update and test masks with `and`. Held input is cleared by reset.

## 8. Timing and Scanout

- Timing derives only from emulated cycle costs and the selected profile.
- Host execution speed does not change program-visible timers.
- Timer values wrap at 65536.
- `current scanline` advances according to `cycles * refresh * displayHeight / clockHz`.
- Crossing a frame boundary refreshes the full framebuffer and increments the frame counter.
- Between frame boundaries, completed scanlines are rendered progressively.
- A write to `0xFF12` resets scanline phase and current line to zero, then renders line zero.
- Repeated `%present` in a tight idle loop can trap visible scanout near the top.
- At `1x`, host time follows the hardware clock. Desktop speed ranges from `0.05x` to `10x` plus Unlimited.
- Audio is resampled with speed, changing playback speed/pitch. Unlimited disables host audio output.

Use subtraction for wrap-safe intervals shorter than 65536 units:

```text
%frames r10
wait:
    %frames r0
    sub r1, r0, r10
    movi r2, 2
    jlt r1, r2, wait
```

## 9. Graphics

### RGB332

```text
bits 7-5 = red 0-7
bits 4-2 = green 0-7
bits 1-0 = blue 0-3
```

Host conversion expands each component linearly to 0-255. When `paletteColors <= 32`, the implementation constrains every graphics color with `color & 0xDA`. This is a fixed bit mask, not nearest-color palette matching. Profiles above 32 colors use the full RGB332 byte.

### PPU Modes

- `FF10 = 0`: display disabled; rendered output is black.
- `FF10 = 1`: display enabled, bitmap mode.
- `FF10 = 3`: display enabled, tile mode.
- Other values follow the same two low bits; higher bits currently have no effect.

### Bitmap Mode

Physical VRAM offset:

```text
pixel = y * displayWidth + x
bank = pixel / 8192
CPU address = 0xC000 + (pixel % 8192)
```

One RGB332 byte represents one pixel. Pocket Color requires 23040 bytes, spanning VRAM banks 0, 1, and 2.

### Tile Mode

- Tiles are always 8x8 pixels and 64 RGB332 bytes.
- Up to 256 tile indices fit in one byte.
- Tile `N` begins at physical VRAM offset `N * 64`.
- Tile map begins at physical offset `0x4000`.
- Map width is `(displayWidth + 7) / 8`.
- Pixel lookup uses map cell `(x/8, y/8)` and tile pixel `(x%8, y%8)`.
- Pocket Color map dimensions are 20x18 and occupy 360 bytes.
- Physical offset `0x4000` appears at CPU `0xC000` with VRAM bank 2.

### Sprites

- Sprite table begins at physical VRAM offset `0x5000`.
- It appears at CPU `0xD000` with VRAM bank 2.
- Descriptors are six bytes and contain:

| Byte | Field |
| ---: | --- |
| 0 | X low byte |
| 1 | X high byte |
| 2 | Y low byte |
| 3 | Y high byte |
| 4 | RGB332 color |
| 5 | Square size |

- Descriptor coordinates are little-endian, unlike `sti`/`ldi` words.
- Size is clamped to 1-32 while rendering.
- Sprites are solid-color squares; there are no sprite textures, transparency, flipping, priority flags, or rotation.
- Sprites are rendered after bitmap/tile background and therefore overwrite it.
- X pixels beyond the right edge are clipped.
- A sprite outside the current scanline is ignored.
- Sprites beyond the per-scanline limit are silently skipped for that line.
- A total active count above the profile maximum faults when a scanline renders.
- Descriptor data beyond allocated VRAM stops descriptor iteration.

The sprite count register is little-endian. Set both bytes:

```text
movi r0, 10
stbi r0, 0xFF13
movi r0, 0
stbi r0, 0xFF14
```

## 10. Audio

- Square-wave synthesis at a 48000 Hz host sample rate.
- Each channel has enabled, 16-bit frequency, 8-bit volume, and phase.
- Frequency is little-endian in `FF42-FF43`.
- A channel with disabled state, zero volume, or zero frequency is silent.
- Active channels are averaged, preventing simple additive clipping.
- Selecting a channel wraps modulo available channel count.
- With zero configured channels, audio reads return zero and writes have no audible effect.
- There are no envelopes, duration timers, wave types, noise, panning, or sample playback. Programs must change or disable channels themselves.

## 11. Storage and Assets

- Persistent storage is raw profile-sized binary data.
- Reset does not clear storage.
- Configure/reset does recreate storage; loading a project profile therefore starts with zero storage until a file is loaded.
- Loading a short file zero-fills the remaining storage.
- Saving writes the entire configured storage size.
- Programs should store magic bytes and a version before payload data.

`CPU-ASM-ASSET input.ppm output.rgb332` converts P3/P6 PPM pixels to raw RGB332 bytes. Desktop import loads converted bytes at physical VRAM offset zero and restarts scanout. It does not configure PPU mode or validate image dimensions against the display.

## 12. Reset and Machine-State Quirks

Reset performs all of the following:

- Clears all 32 registers.
- Clears RAM and VRAM.
- Does not clear ROM or persistent storage.
- Sets PC to the assembler/tool entry point.
- Sets SP to `0xFFFE`.
- Resets all bank selectors to zero.
- Sets PPU control to bitmap-enabled value 1.
- Clears sprite count, input queue, held buttons, output text, timers, audio state, and fault state.
- Refreshes the framebuffer from cleared VRAM.

Loading a ROM zero-fills the profile ROM allocation, copies the image, records its actual byte length for bank calculations, and resets the machine.

`hlt` is not an idle instruction that can later continue; it enters Halted state. Use `jmp idle` for a live idle loop when scanout/audio/timers must continue.

## 13. Fault Codes

| Code | Name | Cause |
| ---: | --- | --- |
| 0 | None | Normal operation |
| 1 | InvalidRegister | Encoded register index outside 0-31 |
| 2 | DivisionByZero | `div` or `mod` divisor is zero |
| 3 | UnknownOpcode | Instruction opcode is not implemented |
| 4 | RomTooLarge | Loaded image exceeds selected profile ROM |
| 5 | InvalidProfile | Defined code; profile validation currently rejects configuration before execution |
| 6 | ProgramCounterOutsideRom | Defined code but not currently emitted; out-of-image ROM fetches return zero |
| 7 | PpuLimitExceeded | Total active sprite count exceeds profile limit |

Faulting stops execution and records a human-readable message. `FF31` exposes the numeric code.

## 14. Project File Format

Minimal project:

```json
{
  "name": "My Pocket Game",
  "source": "examples/my-game.asm",
  "storage": "my-game.sav",
  "asset": "",
  "profile": "pocket"
}
```

Recognized profile strings are `pocket`, `home`, and `studio`; any other value falls back to Pocket. `studio` means Custom Hardware. Optional custom fields are read by key name:

```json
{
  "studio": {
    "clockHz": 4000000,
    "displayWidth": 160,
    "displayHeight": 144,
    "ramBytes": 262144,
    "romBytes": 1048576,
    "vramBytes": 65536,
    "storageBytes": 65536,
    "maxSprites": 40,
    "spritesPerScanline": 10,
    "paletteColors": 32,
    "audioChannels": 4,
    "framesPerSecond": 30
  }
}
```

Input keys are `keyUp`, `keyDown`, `keyLeft`, `keyRight`, `keyA`, `keyB`, `keyStart`, `keySelect` and matching `pad...` names. Values are SDL keycodes/button identifiers. Missing fields retain built-in defaults.

The project loader is intentionally simple and searches text for named JSON string/integer fields. Use ordinary generated JSON without comments, expressions, negative integer settings, or duplicate keys.

## 15. CLI and Desktop Workflow

Build desktop tools:

```sh
cmake -S . -B build-console -DCMAKE_BUILD_TYPE=Release -DCPU_ASM_BUILD_DESKTOP=ON
cmake --build build-console --parallel
```

Assemble a ROM:

```sh
./build-console/CPU-ASM-EMULATOR --asm game.asm --bin game.rom
```

Useful CLI options:

- `--asm path`: assemble source.
- `--bin path`: write complete ROM bytes.
- `--outbin`: print assembled ROM bytes as binary text.
- `--emulate`: run through the legacy CLI CPU path.
- `--rom path`: load a binary ROM instead of assembling; cannot be combined with `--asm`.
- `--nodefaults`: disable built-in macros.
- `--verbose`: print assembly details.

Use `CPU-ASM-CONSOLE` for profile-aware graphics, audio, controls, inspectors, execution speed, and source debugging. `Run` assembles, loads, resets, and starts. `Step` rebuilds first when source is dirty. The desktop does not automatically save persistent storage; use Load Storage and Save Storage.

## 16. Code-Generation Rules

To maximize the chance that a generated game works:

1. Target Pocket Color unless another profile is explicitly required.
2. Start with `.reset start` and `.org 8`.
3. Keep executable code and small strings below logical `0x8000`.
4. Use `.rombank` only for larger read-only tables/assets and select the bank before reads.
5. Document every RAM address and whether it is a byte or word.
6. Treat word memory as big-endian, but sprite coordinates, sprite count, and audio frequency as explicitly little-endian device formats.
7. Initialize VRAM before enabling/presenting the display.
8. In tile mode, write tiles in VRAM banks 0-1 and map/sprites through bank 2 for Pocket Color.
9. Read held input from `FF22`; use `in`/`inkey` only for queued events.
10. Gate gameplay with `%frames` or `%millis`, never host-speed busy loops.
11. Present after meaningful rendering work, not continuously while idle.
12. Set both sprite-count bytes and remain below total/per-line limits.
13. Balance every `push` with `pop` and return from calls with `ret`.
14. Avoid semicolons inside strings and avoid macro recursion.
15. Use an infinite jump instead of `hlt` when devices must keep advancing.
16. Add a profile check when dimensions or limits are hard-coded.
17. Test assembly, runtime fault state, held input, PPU mode, sprite count, and storage format.
18. Test at desktop `1x` before considering real-time behavior correct.

## 17. Working Reference Programs

- `examples/console/manual/01-star-catcher.asm`: recommended compact game structure.
- `examples/console/manual/02-neon-dodge.asm`: multiple sprites, collision, states, ROM banking, layered audio.
- `examples/console/manual/03-chunk-raytracer.asm`: bitmap addressing, multi-bank VRAM, progressive rendering.
- `examples/console/snake.asm`: larger tile/sprite game using held controls and frame timing.

For explanatory material, see `docs/manual/README.md`. For the shorter authoritative architecture contract, see `docs/experimental-architecture.md`.
