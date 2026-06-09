# CPU ASM Console Programming Manual

This manual teaches programmers who are new to assembly how to create complete programs for the experimental CPU ASM console. The main path targets **Pocket Color**, the smallest fixed hardware profile, because its limits make every hardware concept visible.

You will begin with text output and CPU fundamentals, draw directly into video memory, add tiles, sprites, controls, timing, sound, and storage, and then build the complete Star Catcher game. The final chapters examine Neon Dodge and Chunk Raytracer and show how to design an original project.

## How To Use This Manual

1. Build and open the desktop development environment.
2. Read the chapters in order through Star Catcher.
3. Type and run the short listings instead of only reading them.
4. Use Pause, Step, breakpoints, and the inspectors whenever a result is surprising.
5. After Star Catcher, read the case studies in whichever order interests you.

The code assumes the built-in macro library is enabled. The CLI enables it by default; do not pass `--nodefaults` while following the tutorial.

## Chapters

1. [Welcome and Mental Model](01-welcome.md)
2. [Build, Run, and Debug](02-workflow.md)
3. [Assembly Foundations](03-assembly-foundations.md)
4. [The Console Hardware](04-console-hardware.md)
5. [Graphics: Bitmap, Tiles, and Sprites](05-graphics.md)
6. [Input, Timing, Audio, and Storage](06-devices.md)
7. [Build Star Catcher](07-star-catcher.md)
8. [Advanced Example Case Studies](08-case-studies.md)
9. [Design Your Own Program](09-original-programs.md)
10. [Debugging and Optimization](10-debugging.md)
11. [Reference Appendices](11-reference.md)

## Complete Examples

| Example | Main topics |
| --- | --- |
| [`01-star-catcher.asm`](../../examples/console/manual/01-star-catcher.asm) | Tile mode, two sprites, held input, score state, timing, audio |
| [`02-neon-dodge.asm`](../../examples/console/manual/02-neon-dodge.asm) | Multiple sprites, collision, game states, ROM banks, four-channel audio |
| [`03-chunk-raytracer.asm`](../../examples/console/manual/03-chunk-raytracer.asm) | Bitmap mode, RGB332 shading, banked VRAM, progressive scanout |

The authoritative hardware contract is the [Experimental Console Architecture](../experimental-architecture.md). This manual explains how to use that contract; the architecture document defines it.

[Next: Welcome and Mental Model](01-welcome.md)
