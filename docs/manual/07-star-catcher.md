# 7. Build Star Catcher

[Previous: Input, Timing, Audio, and Storage](06-devices.md) | [Manual index](README.md) | [Next: Advanced Case Studies](08-case-studies.md)

This chapter constructs the systems in [`01-star-catcher.asm`](../../examples/console/manual/01-star-catcher.asm). Keep the complete source open while reading. The goal is to understand why each section exists, not to memorize every address.

## Step 1: Define the Game

Star Catcher has one player-controlled catcher and one falling star. Catching the star increases a score. Missing it simply respawns it. The game never ends, which keeps the first complete project focused on the normal update loop.

State fits in fixed RAM:

| Address | Value |
| --- | --- |
| `0000` | Catcher X, byte |
| `0001` | Star X, byte |
| `0002` | Star Y, byte |
| `0004-0005` | Score, word |

Leaving `0003` unused keeps the score word aligned and prevents accidental overlap.

## Step 2: Boot Deliberately

The source begins with a reset vector, reserves the first instruction slot, and validates the hardware profile:

```text
.reset start
.org 8

start:
    ldbi r0, 0xFF30
    jz r0, pocket_ok
    %println r0 wrong_profile_text
    hlt
```

After validation, initialization follows a useful order:

1. Build immutable tile graphics.
2. Build the tile map.
3. Initialize mutable game state and sprites.
4. Enable tile mode.
5. Capture the current frame timer.
6. Enter the main loop.

Preparing VRAM before enabling the display avoids presenting partially initialized graphics.

## Step 3: Build the Tiles

The game uses three solid tiles:

- Tile 0: dark playfield.
- Tile 1: empty HUD stripe.
- Tile 2: filled score segment.

Each tile is 64 bytes. `fill_tile_range` accepts start address, end address, and color. It is reusable because the loop’s policy is independent of the specific tile.

This is an important assembly design technique: move repeated mechanisms into subroutines, while callers supply values through documented registers.

## Step 4: Build the Map

VRAM bank 2 exposes the tile map at `0xC000`. The program fills all 360 map cells with tile 0, then overwrites the final row with tile 1.

The bottom row begins at:

```text
17 * 20 = 340 = 0x154
0xC000 + 0x154 = 0xC154
```

That calculation explains the constants `0xC154` and `0xC168`. Write derivations like this in comments whenever an address depends on a screen layout.

## Step 5: Initialize State

`new_game` gives the catcher and star starting positions, clears the score, initializes descriptors, draws the HUD, and plays a start sound.

Keeping initialization in one callable subroutine makes later restart support easy. Neon Dodge uses the same pattern for its game-over flow.

## Step 6: Describe the Sprites

Star Catcher uses two descriptors in VRAM bank 2:

- Catcher at `0xD000`.
- Star at `0xD006`.

Position-update subroutines write only coordinates. Color and size are initialized once. This reduces work in the main loop and avoids rewriting constant data every frame.

The catcher is 15 pixels square at Y 128. The star is 7 pixels square. These values are descriptor sizes in this implementation, not “size minus one.”

## Step 7: Establish a Frame Loop

The loop waits until the completed-frame counter changes:

```text
main_loop:
    %frames r0
    je r0, r28, main_loop
    mov r28, r0

    call poll_controls
    call update_star
    call update_hud
    %present r0
    jmp main_loop
```

This separates emulation speed from game speed. At `0.05x`, frames arrive slowly in wall time. At `10x`, they arrive quickly. The game still performs one logical update per emulated frame.

`r28` stores the last processed frame. `r29` stores the last movement frame. Reserve high registers for long-lived loop state only when subroutines consistently avoid or preserve them.

## Step 8: Read Held Controls

Movement normally happens every two frames. Holding A permits movement every frame. The code first reads the held mask, tests A, and only applies the two-frame delay when A is not held.

Left movement clamps at 0. Right movement clamps near the right edge so the catcher remains visible. Every successful move updates the sprite descriptor immediately.

The branch structure gives Left priority if both Left and Right are held. This is a design choice. A larger game might explicitly cancel opposite directions instead.

## Step 9: Move and Respawn the Star

The star’s speed is:

```text
1 + (score modulo 3)
```

This creates three difficulty levels without a table. After movement, the game checks for a catch near the catcher, then checks whether the star passed the bottom.

Respawn X uses the low cycle timer plus score-derived variation:

```text
(cycles + score * 19) modulo 152
```

This is deterministic, not true randomness. The exact sequence depends on when catches and misses occur, which is sufficient for a small game.

## Step 10: Detect Overlap

Both objects are axis-aligned squares. Near the bottom, horizontal overlap exists unless either object is completely to one side:

```text
star_right < catcher_left
catcher_right < star_left
```

If neither rejection is true, the objects overlap. Collision routines are often easier to write as early rejection tests than as one large condition.

On a catch, increment the 16-bit score, play the catch sound, and respawn. The caller continues safely because `respawn_star` also updates the sprite.

## Step 11: Draw the HUD

Every frame, the bottom row is reset to tile 1. Then `score modulo 20` cells are replaced by tile 2. This is intentionally simple and makes the score visible without a font renderer.

The update does at most 40 tile-map writes per frame, which is affordable on Pocket Color. If the HUD were larger, track whether score changed and redraw only when necessary.

## Step 12: Add Sound

The project uses separate channels for start, catch, and miss sounds. Each event selects a channel and changes its frequency and volume. Because channels remain enabled, later events replace settings rather than scheduling envelopes.

For a more polished game, store an expiration frame per channel and call `%toneoff` when the duration elapses.

## Step 13: Test the Complete Game

Load `examples/console/manual/01-star-catcher.console.json`, Run, and verify:

1. The catcher moves with Left and Right.
2. Holding A increases movement rate.
3. The star respawns after a catch or miss.
4. The bottom bar grows with score modulo 20.
5. Audio events differ.
6. The machine remains within two sprites and never faults.

Then pause and inspect fixed RAM, VRAM bank 2 around `0xD000`, and the frame timer registers. Connect each visible behavior to its stored bytes.

## Extensions

Try one change at a time:

- Add lives and a game-over flag.
- Move the catcher vertically with Up and Down.
- Add a second star while respecting scanline limits.
- Store the high score in persistent storage.
- Use ROM-bank tables for a difficulty curve.
- Stop event sounds after a fixed number of frames.

[Next: Advanced Case Studies](08-case-studies.md)
