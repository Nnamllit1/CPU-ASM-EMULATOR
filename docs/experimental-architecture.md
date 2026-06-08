# Experimental Console Architecture

This document defines the hardware contract used by the `ai-console-experiment` branch. It is an original design built around the project's existing 16-bit CPU and instruction encoding.

## Profiles

| Profile | CPU | Display | RAM | ROM | VRAM | Storage | Sprites | Audio |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Pocket Color | 4 MHz | 160x144 | 256 KiB | 1 MiB | 64 KiB | 64 KiB | 40, 10/scanline | 4 square-wave channels |
| Home 16 | 8 MHz | 320x240 | 1 MiB | 4 MiB | 256 KiB | 1 MiB | 128, 32/scanline | 8 square-wave channels |
| Studio | 1 Hz-1 GHz | 64x64-3840x2160 | 64 KiB-256 MiB | 64 KiB-512 MiB | framebuffer-256 MiB | 0-256 MiB | configurable | configurable |

All profiles execute the same instruction set. Projects select a profile at runtime and may be tested under another profile without rebuilding.

## Address Map

| Address | Purpose |
| --- | --- |
| `0000-7FFF` | Fixed RAM |
| `8000-BFFF` | 16 KiB banked RAM window |
| `C000-DFFF` | 8 KiB banked VRAM window |
| `E000-FEFF` | 7936-byte banked persistent-storage window |
| `FF00-FFFF` | Device registers |

Instruction ROM is Harvard-style. `0000-7FFF` is fixed and `8000-FFFF` is a switchable 32 KiB ROM bank.

## Device Registers

| Address | Name | Description |
| --- | --- | --- |
| `FF00` | RAM bank | Select the RAM window bank |
| `FF01` | VRAM bank | Select the VRAM window bank |
| `FF02` | ROM bank | Select the upper instruction-ROM bank |
| `FF03` | Storage bank | Select the persistent-storage window bank |
| `FF10` | PPU control | Bit 0 enables display output |
| `FF11` | PPU status | Bit 0 is set at the frame boundary |
| `FF12` | PPU present | Any write immediately refreshes the host display |
| `FF13-FF14` | Sprite count | Little-endian number of active sprite descriptors |
| `FF20` | Input status | Nonzero when queued input is available |
| `FF21` | Input data | Read current input; write to consume it |
| `FF22-FF23` | Console buttons | Little-endian held-button mask: Up, Down, Left, Right, A, B, Start, Select in bits 0-7 |
| `FF30` | Profile ID | `0` Pocket, `1` Home, `2` Studio |
| `FF31` | Fault code | Current deterministic hardware fault |
| `FF40` | Audio channel | Selected audio channel index |
| `FF41` | Audio control | Bit 0 enables the selected square-wave channel |
| `FF42-FF43` | Audio frequency | Little-endian frequency in Hz |
| `FF44` | Audio volume | Channel volume from 0 to 255 |

PPU control bit 1 selects tile mode. Framebuffer mode uses one RGB332 byte per pixel at the start of VRAM. Tile mode uses 256 8x8 RGB332 tiles at `VRAM 0000-3FFF` and a tile map at `VRAM 4000`. Sprite descriptors begin at `VRAM 5000` and contain little-endian X, little-endian Y, RGB332 color, and square size. Pocket Color quantizes output to 32 colors; every profile enforces total and per-scanline sprite limits.

The assembly convenience library writes the same registers and VRAM used by direct hardware access.

## Assets And Storage

`CPU-ASM-ASSET input.ppm output.rgb332` converts P3/P6 PPM images to one-byte RGB332 pixels. The desktop environment can import the same files directly into VRAM. Persistent storage is saved as a raw profile-sized image and accessed through the banked `E000-FEFF` window.

## Desktop Development Environment

The SDL3 desktop target provides a dockable assembly editor, console display, source-level debugger, build/program output, and RAM/VRAM/ROM/storage/device inspectors. Project, custom-hardware, control, asset, and appearance settings open in a separate native SDL window. `Run` assembles the current editor contents, loads a fresh ROM, resets the machine, and starts execution. Editor-gutter breakpoints and the highlighted execution line use assembler-generated source mappings, including mappings from expanded macro instructions back to their invocation line.

Keyboard, SDL gamepad, and on-screen controls feed the same eight logical console buttons. Projects can persist keyboard and gamepad bindings. Button holds are exposed through `FF22-FF23`, while the existing queued input registers and `in`/`inkey` instructions remain available for event-oriented input.

## Timing

Execution is deterministic. ALU instructions cost one cycle, taken branches cost two, memory operations cost two, stack/call operations cost three, and device I/O costs four. Frame boundaries derive from profile clock speed and frame rate, not host speed.
