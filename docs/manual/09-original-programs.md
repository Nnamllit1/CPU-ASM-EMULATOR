# 9. Design Your Own Program

[Previous: Advanced Case Studies](08-case-studies.md) | [Manual index](README.md) | [Next: Debugging and Optimization](10-debugging.md)

## Begin With a One-Sentence Rule

Describe the program without implementation details. For example:

> The player places buildings on a grid, buildings consume power, and the city earns money each simulated day.

This identifies objects, actions, resources, and time. Avoid beginning with “use all hardware features.” Features should support the program rather than dictate unrelated mechanics.

## Convert the Idea Into Systems

Write a table before writing instructions:

| System | Questions |
| --- | --- |
| State | What must survive between frames? |
| Input | Which actions are held, pressed once, or menu-oriented? |
| Update | What changes every frame, tick, or event? |
| Rendering | Bitmap, tiles, sprites, or a combination? |
| Audio | Which events need feedback? |
| Storage | What should survive emulator restarts? |

For a city builder, state might include cursor position, money, day, selected building, and a 20x18 map. Rendering naturally favors tile mode. The cursor can be a sprite. Storage can save the map and economy state.

## Plan Memory Explicitly

Maintain a memory map in comments:

```text
0000-001F  global state
0100-0267  20x18 logical map
0300-03FF  temporary work buffers
8000-BFFF  banked level or simulation data
```

Choose byte or word width for every field. Avoid unexplained addresses scattered throughout code. Define layout comments near startup and keep related fields together.

Use fixed RAM for frequently shared state. Use banked RAM for large maps, history, or infrequently accessed systems. Keep the active bank predictable at subroutine boundaries.

## Choose the Rendering Model

Choose **tile mode** when the world is grid-based, repeated, or mostly static. Choose **bitmap mode** for arbitrary pixels, generated images, or per-pixel effects. Use **sprites** for moving square overlays.

A mixed game usually means tile mode plus sprites, not switching PPU mode every frame. Mode switches are better for distinct screens such as a bitmap title followed by tile gameplay.

## Design the Main Loop

A robust frame-driven loop has clear phases:

```text
wait for new frame
read input
update current game state
resolve collisions/rules
update graphics
update audio timers
present
repeat
```

Do not let rendering silently change gameplay state. Do not read controls in many unrelated subroutines. A consistent phase order prevents one-frame inconsistencies.

For slower simulation, count frames or milliseconds and call the simulation only when its interval elapses. Continue rendering input and animation every frame.

## Budget the Hardware

Pocket Color runs at 4 MHz and 30 Hz, giving roughly 133333 CPU cycles per frame. Not every program needs to consume the whole budget. Device accesses and memory operations cost more than simple ALU instructions.

Estimate work by counting major loops:

- Clearing 23040 bitmap bytes every frame is expensive.
- Updating 360 tile-map bytes only when a map changes is affordable.
- Updating a few sprite coordinates every frame is cheap.
- Per-pixel division and modulo are structurally expensive even though each ALU instruction currently costs one emulated cycle.

Measure with `%cycles` around a subroutine. Store the starting value, call the work, read the timer again, and subtract.

## Use Milestones

Build the smallest observable version first:

1. Boot and print profile confirmation.
2. Enable a renderer and show a static screen.
3. Add one controllable object.
4. Add one rule or collision.
5. Add score or resources.
6. Add sound.
7. Add storage.
8. Add more content only after the loop is stable.

Each milestone should still assemble and run. This keeps regressions local.

## Organize Subroutines

Good subroutine groups include:

- `initialize_*`
- `poll_*`
- `update_*`
- `draw_*`
- `play_*`
- `load_*` and `save_*`

Document register inputs, outputs, and clobbers. Preserve long-lived registers with push/pop or establish a project-wide calling convention.

## Failure and Recovery

Decide what invalid data means:

- Wrong profile: print a clear message and halt.
- Missing save header: initialize defaults.
- Unsupported save version: ignore or migrate it.
- Impossible state: reset the subsystem or fault during development.
- Sprite pressure: distribute objects vertically, reduce count, or accept skipped sprites deliberately.

## From-Scratch Checklist

Before calling a project complete, verify:

- Reset vector and entry layout are correct.
- The target profile is explicit.
- RAM, ROM-bank, VRAM-bank, and storage layouts are documented.
- Timing uses hardware timers rather than host assumptions.
- Opposing input and button priority are intentional.
- Sprite counts fit total and scanline limits.
- Present calls are placed deliberately.
- Save data has a recognizable format.
- Runtime faults and build diagnostics are clean.
- The program behaves at `1x`, not only Unlimited.

## Suggested Original Projects

- A tile-map paint tool with a sprite cursor and persistent save.
- A rhythm sequencer using four audio channels and frame timing.
- A turn-based tactics board with ROM-banked levels.
- A particle fountain that tests sprite scanline pressure.
- A procedural landscape renderer using bitmap chunks.
- A compact city simulation with tile state, daily timers, and storage.

[Next: Debugging and Optimization](10-debugging.md)
