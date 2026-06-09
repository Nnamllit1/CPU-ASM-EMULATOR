# 5. Graphics: Bitmap, Tiles, and Sprites

[Previous: The Console Hardware](04-console-hardware.md) | [Manual index](README.md) | [Next: Input, Timing, Audio, and Storage](06-devices.md)

## RGB332 Colors

Every graphics byte uses RGB332:

```text
bits 7-5: red   (0-7)
bits 4-2: green (0-7)
bits 1-0: blue  (0-3)
```

The byte `0b11100000`, or `0xE0`, is bright red. `0x1C` is green and `0x03` is blue. A general packed color is:

```text
color = (red << 5) | (green << 2) | blue
```

Pocket Color quantizes output to 32 colors by masking RGB332 values. Choose colors while viewing the Pocket profile rather than assuming all 256 RGB332 combinations remain distinct.

## Bitmap Mode

Bitmap mode treats physical VRAM offset 0 as one byte per display pixel in row-major order:

```text
offset = y * display_width + x
```

This complete listing paints the first 768 pixels as red, green, and blue stripes:

```asm
.reset start
.org 8

start:
    movi r0, 1
    stbi r0, 0xFF10
    movi r0, 0
    stbi r0, 0xFF01

    movi r1, 0xC000
    movi r2, 1
    movi r3, 0xC100
    movi r4, 0xE0
red:
    stb r1, r4
    add r1, r1, r2
    jlt r1, r3, red

    movi r3, 0xC200
    movi r4, 0x1C
green:
    stb r1, r4
    add r1, r1, r2
    jlt r1, r3, green

    movi r3, 0xC300
    movi r4, 0x03
blue:
    stb r1, r4
    add r1, r1, r2
    jlt r1, r3, blue

    %present r0
    hlt
```

For a full Pocket Color framebuffer, divide the physical offset by 8192 to select a VRAM bank, take the offset modulo 8192, then add `0xC000` for the visible CPU address. Chunk Raytracer performs this calculation for every pixel.

Bitmap mode offers arbitrary pixels but costs more memory and CPU time. Prefer it for procedural images, paint-like tools, full-screen effects, and scenes that cannot be described efficiently with tiles.

## Tile Mode

Tile mode divides graphics into reusable 8x8-pixel tiles. Each tile occupies 64 bytes. Tile `n` starts at physical VRAM offset `n * 64`.

The tile map contains one tile number per screen cell. For Pocket Color:

- Width: `160 / 8 = 20` cells.
- Height: `144 / 8 = 18` cells.
- Map size: `360` bytes.
- Physical map start: `0x4000`, visible as `0xC000` in VRAM bank 2.

This complete listing builds a solid dark tile, a cyan tile, and an alternating map:

```asm
.reset start
.org 8

start:
    movi r0, 0
    stbi r0, 0xFF01
    movi r1, 0xC000
    movi r2, 0xC040
    movi r3, 0x00
    movi r4, 1
fill_dark:
    stb r1, r3
    add r1, r1, r4
    jlt r1, r2, fill_dark

    movi r2, 0xC080
    movi r3, 0x12
fill_cyan:
    stb r1, r3
    add r1, r1, r4
    jlt r1, r2, fill_cyan

    movi r0, 2
    stbi r0, 0xFF01
    movi r1, 0xC000
    movi r2, 0xC168
    movi r3, 0
map_loop:
    stb r1, r3
    movi r5, 1
    xor r3, r3, r5
    add r1, r1, r4
    jlt r1, r2, map_loop

    movi r0, 3
    stbi r0, 0xFF10
    %present r0
idle:
    jmp idle
```

Tile mode is ideal for maps, boards, menus, and repeated backgrounds. Updating one tile changes every map cell that references it.

## Sprites

Sprites are colored square overlays. Their descriptors begin at physical VRAM offset `0x5000`, visible at CPU address `0xD000` in VRAM bank 2.

Each six-byte descriptor is:

| Offset | Meaning |
| ---: | --- |
| 0 | X low byte |
| 1 | X high byte |
| 2 | Y low byte |
| 3 | Y high byte |
| 4 | RGB332 color |
| 5 | Size, clamped to 1-32 |

This creates one 8x8 sprite at `(72, 64)`:

```text
movi r0, 2
stbi r0, 0xFF01
movi r0, 72
stbi r0, 0xD000
movi r0, 0
stbi r0, 0xD001
movi r0, 64
stbi r0, 0xD002
movi r0, 0
stbi r0, 0xD003
movi r0, 0xDA
stbi r0, 0xD004
movi r0, 8
stbi r0, 0xD005
movi r0, 1
stbi r0, 0xFF13
movi r0, 0
stbi r0, 0xFF14
```

The sprite count at `0xFF13-0xFF14` is little-endian, unlike words written with `sti`. For fewer than 256 sprites, set the low byte and clear the high byte explicitly.

Pocket Color allows 40 active descriptors and renders at most 10 sprites on one scanline. Extra sprites on a scanline are skipped; exceeding the total profile limit produces a fault.

## Present and Scanout

Writing `0xFF12` restarts progressive scanout and exposes the first row immediately. Present after meaningful rendering work. A frame loop may present once per update. A chunk renderer may present after each chunk to reveal progress.

Do not repeatedly present while waiting. Let emulated cycles advance scanlines. In a finished static renderer, present once and then idle without another present.

## Choosing a Renderer

Use bitmap mode when pixels change independently or the image is generated procedurally. Use tile mode when much of the screen repeats or aligns to an 8x8 grid. Use sprites for moving square objects and tile mode for the background. Star Catcher and Neon Dodge use this latter combination.

[Next: Input, Timing, Audio, and Storage](06-devices.md)
