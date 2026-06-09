# 11. Reference Appendices

[Previous: Debugging and Optimization](10-debugging.md) | [Manual index](README.md)

This is a compact working reference. The root [README](../../README.md) contains opcode numbers, and the [architecture specification](../experimental-architecture.md) remains authoritative for the hardware contract.

## Instruction Summary

| Group | Instructions |
| --- | --- |
| Move | `movi`, `mov`, `movc` |
| Arithmetic | `add`, `sub`, `mul`, `div`, `mod` |
| Shift/bitwise | `shl`, `shr`, `and`, `or`, `xor`, `not` |
| Branch | `jmp`, `jz`, `jnz`, `je`, `jne`, `jlt`, `jle`, `jgt`, `jge` |
| RAM words | `ld`, `ldi`, `st`, `sti` |
| RAM bytes | `ldb`, `ldbi`, `stb`, `stbi` |
| ROM data | `ldbr`, `ldbri`, `ldwr`, `ldwri` |
| Text/input | `out`, `outn`, `outs`, `in`, `inkey` |
| Control/stack | `hlt`, `call`, `ret`, `push`, `pop` |

Destination registers appear first. Arithmetic and comparisons are unsigned 16-bit operations. Word RAM operations are big-endian.

## Directives

| Directive | Purpose |
| --- | --- |
| `.byte value` | Emit one ROM byte |
| `.word value` | Emit one big-endian ROM word |
| `.ascii "text"` | Emit text without a terminator |
| `.asciiz "text"` | Emit text followed by zero |
| `.space count` | Emit zero bytes |
| `.align value` | Pad until address is divisible by value |
| `.org address` | Move output forward to an address |
| `.entry label` | Set emulator entry point |
| `.reset label` | Write reset address to ROM bytes 0-1 |
| `.rombank N` | Begin physical switchable bank `N`, logically at `0x8000` |

## Built-In Macros

| Macro | Purpose |
| --- | --- |
| `%putc reg value` | Print one character |
| `%putn reg value` | Print one immediate number |
| `%space reg` | Print a space |
| `%newline reg` | Print a newline |
| `%print reg label` | Print a zero-terminated ROM string |
| `%println reg label` | Print a ROM string and newline |
| `%read reg` | Poll queued character input |
| `%readkey reg` | Poll queued raw key input |
| `%rambank reg bank` | Select RAM bank |
| `%vbank reg bank` | Select VRAM bank |
| `%rombank reg bank` | Select ROM bank |
| `%storagebank reg bank` | Select storage bank |
| `%ppumode reg mode` | Write PPU control |
| `%present reg` | Restart scanout/present |
| `%sprites reg count` | Set low sprite-count byte |
| `%tone reg channel lo hi volume` | Configure and enable a tone |
| `%toneoff reg channel` | Disable a channel |
| `%millis reg` | Read 16-bit millisecond timer |
| `%frames reg` | Read 16-bit completed-frame timer |
| `%cycles reg` | Read low 16 bits of cycle count |

`%sprites` only writes the low count byte. Clear `0xFF14` when a previous high byte may be nonzero.

## Device Registers

| Address | Device |
| --- | --- |
| `FF00` | RAM bank |
| `FF01` | VRAM bank |
| `FF02` | ROM bank |
| `FF03` | Storage bank |
| `FF10` | PPU control: enable bit 0, tile-mode bit 1 |
| `FF11` | PPU status/frame boundary |
| `FF12` | Present/restart scanout |
| `FF13-FF14` | Little-endian sprite count |
| `FF20` | Queued input status |
| `FF21` | Queued input data/consume |
| `FF22-FF23` | Little-endian held-button mask |
| `FF30` | Profile ID |
| `FF31` | Fault code |
| `FF40` | Selected audio channel |
| `FF41` | Audio enable bit 0 |
| `FF42-FF43` | Little-endian audio frequency |
| `FF44` | Audio volume |
| `FF50-FF51` | Big-endian milliseconds |
| `FF52-FF53` | Big-endian frames |
| `FF54-FF55` | Big-endian cycles |

## Graphics Layout

| Physical VRAM offset | Content |
| --- | --- |
| `0000...` | Bitmap framebuffer, or 256 8x8 tile pixels |
| `4000...` | Tile map |
| `5000...` | Six-byte sprite descriptors |

Pocket Color map dimensions are 20x18. Bitmap dimensions are 160x144, requiring 23040 bytes.

## RGB332 Quick Values

| Value | Approximate color |
| ---: | --- |
| `00` | Black |
| `03` | Blue |
| `08` | Dark green/blue after Pocket quantization |
| `12` | Cyan-green |
| `1C` | Green |
| `49` | Purple/blue |
| `90` | Red-purple |
| `C2` | Red |
| `DA` | Yellow |
| `E0` | Bright red |
| `FF` | White |

Always judge final colors through the target profile's palette quantization.

## Pocket Color Limits

| Item | Value |
| --- | ---: |
| Clock | 4,000,000 Hz |
| Refresh | 30 Hz |
| Approximate cycles/frame | 133333 |
| Display | 160x144 |
| Tile map | 20x18 cells |
| VRAM bank window | 8192 bytes |
| Sprites | 40 total, 10 per scanline |
| Audio channels | 4 |

## Glossary

- **ALU**: CPU arithmetic and bitwise execution unit.
- **Bank**: one physical memory segment selected into a smaller address window.
- **Framebuffer**: one color byte for each display pixel.
- **MMIO**: memory-mapped input/output; device control through addresses.
- **PPU**: picture processing unit, responsible for display scanout.
- **RGB332**: eight-bit color with three red, three green, and two blue bits.
- **Scanline**: one horizontal display row updated during scanout.
- **Sprite**: hardware-rendered square overlay described by six VRAM bytes.
- **Tile**: reusable 8x8 RGB332 image selected by a tile-map byte.
- **VRAM**: video memory used by bitmap, tile, and sprite rendering.

Return to the [manual index](README.md), open a blank assembly file, and build the smallest visible version of your own idea.
