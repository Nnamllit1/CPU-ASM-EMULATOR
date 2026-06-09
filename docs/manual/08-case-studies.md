# 8. Advanced Example Case Studies

[Previous: Build Star Catcher](07-star-catcher.md) | [Manual index](README.md) | [Next: Design Your Own Program](09-original-programs.md)

## Neon Dodge

Open [`02-neon-dodge.asm`](../../examples/console/manual/02-neon-dodge.asm). Neon Dodge extends Star Catcher's architecture rather than replacing it.

### More Objects Without More Code Copies

The game has one player and nine meteors. Meteor descriptors are contiguous and six bytes apart. Initialization and updates iterate with:

- `r1`: meteor index.
- `r2`: descriptor pointer.
- `r4`: descriptor size, six bytes.

The loop calls one subroutine for each descriptor, advances the pointer, and increments the index. This data-oriented pattern scales better than nine separate update blocks.

### Per-Object Variation

Meteor X and initial Y use the object index and cycle timer. Falling speed uses index plus score modulo three. One algorithm therefore produces varied objects without storing a large state structure in fixed RAM.

The descriptor itself stores position, so no duplicate meteor coordinate arrays are needed. This is efficient, but it couples gameplay state to graphics memory. A game requiring invisible or non-square entities should keep authoritative state in RAM instead.

### Game States

RAM byte `0x0002` is a game-over flag. The main loop checks it before normal updates. In game-over state, the player flashes and Start calls `new_game`.

This introduces a simple state machine:

```text
playing -> collision -> game over -> Start -> playing
```

For title screens, pause menus, and level transitions, use one state byte and dispatch to a dedicated loop or update subroutine for each state.

### ROM-Banked Data

Tile colors are emitted after `.rombank 0`. `build_tiles` selects ROM bank 0 before reading the table. This demonstrates the correct separation: executable code remains fixed, while larger read-only content is switchable.

### Hardware Pressure

Ten sprites equal Pocket Color's per-scanline limit. Because meteors may align vertically, this is close to the profile's visible sprite pressure even though the total count remains below 40. Test at `1x` and inspect crowded rows.

### Audio Layers

The startup code enables several low-volume channels, then channel 3 is reused for start, score, and crash events. This creates a background layer plus event feedback while staying within four channels.

## Chunk Raytracer

Open [`03-chunk-raytracer.asm`](../../examples/console/manual/03-chunk-raytracer.asm). It is not a mathematically complete perspective raytracer. It is a procedural orthographic renderer designed to exercise bitmap writes and arithmetic.

### Why Chunks

The image is divided into 8x8 chunks. After each chunk, `%present` restarts scanout. At a slow execution speed, the user sees completed regions appear progressively.

Rendering every pixel before one final present would be simpler but hide progress. Presenting after every pixel would spend excessive device cycles and repeatedly restart scanout. A chunk is the compromise.

### Coordinate State

Chunk coordinates, local coordinates, and current pixel coordinates are stored in fixed RAM. This frees registers for shading calculations and makes the state visible in the debugger.

The nested loop computes:

```text
pixel_x = chunk_x + local_x
pixel_y = chunk_y + local_y
```

Then it calls one shader and one framebuffer writer.

### Sphere Approximation

The sphere test computes squared distance from `(80, 64)`:

```text
dx*dx + dy*dy < 42*42
```

No square root is needed. The difference between radius squared and distance squared becomes a small shade index. Upper-left pixels receive a light bias, producing the appearance of directional lighting.

### Sky, Floor, and Shadow

Pixels above the horizon use a Y-based sky gradient. Pixels below it use an 8x8 checker pattern unless they fall inside an elliptical shadow test. These are independent procedural materials selected by branches.

This is a useful general rendering structure:

1. Determine which object or region owns the pixel.
2. Compute a compact shade or pattern index.
3. Map it to a legal RGB332 color.
4. Write the pixel through the correct VRAM bank.

### Banked Framebuffer Addressing

For each pixel:

```text
offset = y * 160 + x
bank = offset / 8192
window_offset = offset modulo 8192
CPU address = 0xC000 + window_offset
```

This calculation is expensive compared with tile drawing. A faster renderer can process sequential pixels, increment a window address, and switch banks only at `0xE000`.

### Final Scanout Rule

After the final present, the program enters an idle jump that does not present again. Repeated writes to `0xFF12` would continually reset scanout and prevent lower rows from remaining current.

## Comparing the Examples

| Concern | Star Catcher | Neon Dodge | Chunk Raytracer |
| --- | --- | --- | --- |
| Renderer | Tiles + sprites | Tiles + sprites | Bitmap |
| Main state | Fixed RAM | RAM + sprite descriptors | RAM loop coordinates |
| Timing | Frame updates | Frame updates | Work-driven progress |
| Input | Held buttons | Held buttons + restart state | None |
| Audio | Event channels | Background + event channel | None |
| Banking | VRAM | VRAM + ROM | Multi-bank VRAM |

[Next: Design Your Own Program](09-original-programs.md)
