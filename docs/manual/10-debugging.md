# 10. Debugging and Optimization

[Previous: Design Your Own Program](09-original-programs.md) | [Manual index](README.md) | [Next: Reference Appendices](11-reference.md)

## Separate Build Errors From Runtime Errors

Assembler errors happen before a ROM is loaded. Check spelling, operand counts, register names, labels, directives, macro arguments, and ROM layout.

Runtime errors happen after a successful build. The machine may fault, halt unexpectedly, draw incorrect data, or continue with wrong state. Use breakpoints and inspectors rather than changing code blindly.

## Debug From the Boundary

Find the first point where correct state becomes incorrect:

1. Break before the relevant subroutine.
2. Record input registers and memory.
3. Step through one branch or loop iteration.
4. Inspect the output address or register.
5. Repeat at the next boundary only if this block is correct.

This is faster than stepping through an entire frame.

## Common Problems

### Nothing Appears

- Confirm `0xFF10` bit 0 is enabled.
- Confirm the selected display mode matches the data layout.
- Confirm the active VRAM bank before writes.
- Present once after initial graphics are ready.
- Check whether the program halted before scanout advanced.

### Only the Top of the Screen Updates

The program is probably writing `0xFF12` repeatedly in a tight loop. Every write restarts scanout at row zero. Present after meaningful work, then allow cycles to pass.

### Tiles Show the Wrong Pattern

- Verify tile number times 64 points to initialized graphics.
- Verify the map is written through VRAM bank 2.
- For non-Pocket displays, calculate map width as `(width + 7) / 8`.
- Check that tile and map data do not overlap physically.

### Sprites Disappear

- Check sprite count at `0xFF13-0xFF14`.
- Check six-byte descriptor alignment.
- Check Y and size against the current scanline.
- Pocket Color renders at most ten sprites per scanline.
- Total count above 40 faults on Pocket Color.

### Input Feels Too Fast

Movement is probably happening once per CPU loop rather than once per frame or timed interval. Gate it with `%frames` or `%millis`.

### Calls Return to the Wrong Place

Check every push/pop pair and every path through a subroutine. Do not jump out of a subroutine after pushing registers without restoring them. Use `ret`, not a hard-coded jump to a caller.

### Banked Data Looks Random

Inspect `0xFF00-0xFF03`. A subroutine may have changed a bank and not restored or documented it. Select the required bank immediately before a critical read or write.

### Saved Data Is Empty Next Time

The program changes emulated storage, but the host must save the backing image. Confirm project storage path, load before execution, and save after modification.

## Fault Codes

| Code | Meaning | Typical cause |
| ---: | --- | --- |
| 0 | None | Normal operation |
| 1 | Invalid register | Corrupt instruction or invalid encoding |
| 2 | Division by zero | Zero divisor in `div` or `mod` |
| 3 | Unknown opcode | Corrupt ROM or execution in data |
| 4 | ROM too large | Image exceeds selected profile |
| 5 | Invalid profile | Custom limits fail validation |
| 6 | PC outside ROM | Bad jump/return or missing code |
| 7 | PPU limit exceeded | Active sprite count exceeds profile maximum |

## Measure Before Optimizing

Use `%cycles` around a routine:

```text
%cycles r20
call expensive_work
%cycles r21
sub r22, r21, r20
```

Inspect `r22` after pausing. The subtraction is wrap-safe when the measured work consumes fewer than 65536 cycles.

## High-Value Optimizations

1. Avoid redrawing unchanged maps or HUDs.
2. Move loop constants out of inner loops.
3. Process sequential bitmap bytes instead of dividing for every pixel.
4. Read a device register once and test the saved value.
5. Keep frequently used state in fixed RAM or registers.
6. Replace repeated code with a subroutine when call cost is smaller than duplication pressure.
7. Use lookup tables in ROM when they replace complicated repeated calculations.

Optimization can trade speed, ROM size, RAM use, and clarity. Preserve a clear version until measured behavior proves it insufficient.

## Test Different Speeds and Profiles

Slow speed reveals scanout and ordering mistakes. `1x` validates intended play. Faster modes expose timer assumptions. Unlimited is useful for completion testing but not real-time feel.

After Pocket Color works, test Home 16 or a Custom Hardware profile only if the program computes dimensions dynamically or deliberately supports those profiles. Hard-coded Pocket layouts should reject incompatible profiles rather than draw out of bounds.

[Next: Reference Appendices](11-reference.md)
