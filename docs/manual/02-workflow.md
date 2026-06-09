# 2. Build, Run, and Debug

[Previous: Welcome and Mental Model](01-welcome.md) | [Manual index](README.md) | [Next: Assembly Foundations](03-assembly-foundations.md)

## Build the Tools

Linux requires CMake 3.20 or newer and a C++20 compiler:

```sh
cmake -S . -B build-console -DCMAKE_BUILD_TYPE=Release -DCPU_ASM_BUILD_DESKTOP=ON
cmake --build build-console --parallel
```

The important executables are:

- `build-console/CPU-ASM-CONSOLE`: desktop editor, emulator, and debugger.
- `build-console/CPU-ASM-EMULATOR`: command-line assembler and original emulator.
- `build-console/CPU-ASM-ASSET`: PPM-to-RGB332 asset converter.

Start the desktop environment from the repository root:

```sh
./build-console/CPU-ASM-CONSOLE
```

## The Desktop Workspace

The default layout contains five panes:

- **Assembly Editor**: source editing, saving, execution controls, and gutter breakpoints.
- **Console**: framebuffer, hardware/profile status, scanline progress, and controls.
- **Debugger**: CPU state, registers, and disassembly.
- **Inspector**: RAM, VRAM, ROM, storage, device registers, and raw graphics data.
- **Output**: build diagnostics and text printed by the program.

Use `View > Reset workspace layout` if panes become difficult to find.

## Run and Step

`Run` performs the complete development cycle in one action:

1. Assemble the current editor text.
2. Load a fresh ROM.
3. Reset the machine.
4. Start execution.

`Step` also assembles and loads first when the source is modified or has not been built. It then executes one instruction. `Pause` stops continuous execution, `Continue` resumes it, and `Reset` restores the loaded ROM to its reset state.

The Build tab in Output reports assembler diagnostics. A successful assembly can still fail at runtime, so also watch the machine state and fault information.

## Source-Level Debugging

Click the editor gutter beside an instruction to add a breakpoint. Breakpoints are resolved through the assembler's source map. Macro-expanded instructions still point back to the macro invocation line.

When execution pauses, the current source line is highlighted. Inspect:

- Registers to confirm intermediate values.
- Disassembly to see encoded instructions and addresses.
- RAM for game state.
- VRAM for pixels, tiles, maps, and sprite descriptors.
- Device registers for selected banks and PPU state.

A productive debugging loop is: place a breakpoint before the wrong behavior, Run, inspect state, then Step through the smallest relevant block.

## Execution Speed

The logarithmic speed control ranges from `0.05x` to `10x`:

- Use `1x` to judge real hardware behavior.
- Use slow speeds to observe scanline updates and animation timing.
- Use faster speeds to reach later game states.
- Use Unlimited for heavy non-interactive work such as the raytracer. Host audio is disabled in this mode.

Changing speed does not change values read from `%frames`, `%millis`, or `%cycles`. It only changes how quickly emulated cycles are consumed relative to wall time.

## Projects and Settings

Open `Project > Settings` to use the separate settings window. The Project tab selects Pocket Color, Home 16, or Custom Hardware and stores source, storage, and asset paths. Controls and asset settings are also kept there.

Load one of the manual projects by entering its path in the Project field:

```text
examples/console/manual/01-star-catcher.console.json
```

Then press **Load Project**. The project selects Pocket Color and loads its source path. Project files contain paths and configuration; the assembly source remains a normal text file.

## Command-Line Assembly

Assemble a source file into a ROM image:

```sh
./build-console/CPU-ASM-EMULATOR \
  --asm examples/console/manual/01-star-catcher.asm \
  --bin star-catcher.rom
```

For small text programs, assemble and run through the legacy command-line emulator:

```sh
./build-console/CPU-ASM-EMULATOR --asm program.asm --emulate
```

The console graphics examples should normally be run in `CPU-ASM-CONSOLE`, which supplies the hardware profile and display.

## A Safe Editing Routine

1. Save the source to a meaningful `.asm` path.
2. Select Pocket Color unless the program deliberately targets another profile.
3. Run after each small behavioral change.
4. Read build diagnostics before debugging runtime behavior.
5. Test at `1x` before considering timing complete.
6. Keep a breakpoint near initialization while designing memory layouts.

[Next: Assembly Foundations](03-assembly-foundations.md)
