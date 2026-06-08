# Experimental Console Architecture

This document defines the hardware contract used by the `ai-console-experiment` branch. It is an original design built around the project's existing 16-bit CPU and instruction encoding.

## Profiles

| Profile | CPU | Display | RAM | ROM | VRAM | Sprites | Audio |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Pocket Color | 4 MHz | 160x144 | 256 KiB | 1 MiB | 64 KiB | 40, 10/scanline | 4 planned channels |
| Home 16 | 8 MHz | 320x240 | 1 MiB | 4 MiB | 256 KiB | 128, 32/scanline | 8 planned channels |
| Studio | 1 Hz-1 GHz | 64x64-3840x2160 | 64 KiB-256 MiB | 64 KiB-512 MiB | framebuffer-256 MiB | configurable | configurable |

All profiles execute the same instruction set. Projects select a profile at runtime and may be tested under another profile without rebuilding.

## Address Map

| Address | Purpose |
| --- | --- |
| `0000-7FFF` | Fixed RAM |
| `8000-BFFF` | 16 KiB banked RAM window |
| `C000-DFFF` | 8 KiB banked VRAM window |
| `E000-FEFF` | Fixed RAM |
| `FF00-FFFF` | Device registers |

Instruction ROM is Harvard-style. `0000-7FFF` is fixed and `8000-FFFF` is a switchable 32 KiB ROM bank.

## Device Registers

| Address | Name | Description |
| --- | --- | --- |
| `FF00` | RAM bank | Select the RAM window bank |
| `FF01` | VRAM bank | Select the VRAM window bank |
| `FF02` | ROM bank | Select the upper instruction-ROM bank |
| `FF10` | PPU control | Bit 0 enables display output |
| `FF11` | PPU status | Bit 0 is set at the frame boundary |
| `FF12` | PPU present | Any write immediately refreshes the host display |
| `FF13-FF14` | Sprite count | Little-endian number of active sprite descriptors |
| `FF20` | Input status | Nonzero when queued input is available |
| `FF21` | Input data | Read current input; write to consume it |
| `FF30` | Profile ID | `0` Pocket, `1` Home, `2` Studio |
| `FF31` | Fault code | Current deterministic hardware fault |

PPU control bit 1 selects tile mode. Framebuffer mode uses one RGB332 byte per pixel at the start of VRAM. Tile mode uses 256 8x8 RGB332 tiles at `VRAM 0000-3FFF` and a tile map at `VRAM 4000`. Sprite descriptors begin at `VRAM 5000` and contain little-endian X, little-endian Y, RGB332 color, and square size. Pocket Color quantizes output to 32 colors; every profile enforces total and per-scanline sprite limits.

The assembly convenience library writes the same registers and VRAM used by direct hardware access.

## Timing

Execution is deterministic. ALU instructions cost one cycle, taken branches cost two, memory operations cost two, stack/call operations cost three, and device I/O costs four. Frame boundaries derive from profile clock speed and frame rate, not host speed.
