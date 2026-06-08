; Pocket Snake
; Target: Pocket Color, 4 MHz, 160x144, 32 colors, 40 sprites.
;
; Controls: D-pad / arrow keys. Press Start or Enter after a crash.
; The board uses 8x8 tiles (20x18 cells). The snake and food are hardware
; sprites, with body coordinates stored in fixed RAM.

start:
    ; Tile mode: bit 0 enables the PPU, bit 1 enables 8x8 tiles.
    movi r0, 3
    stbi r0, 0xFF10

    ; Build two tiles in VRAM bank 0.
    ; Tile 0 is the dark playfield and tile 1 is the cyan wall.
    movi r0, 0
    stbi r0, 0xFF01
    movi r1, 0xC000
    movi r2, 0x00
    movi r3, 1
    movi r4, 0xC040
fill_floor_tile:
    stb r1, r2
    add r1, r1, r3
    jlt r1, r4, fill_floor_tile

    movi r2, 0x12
    movi r4, 0xC080
fill_wall_tile:
    stb r1, r2
    add r1, r1, r3
    jlt r1, r4, fill_wall_tile

    ; Build the 20x18 tile map in VRAM bank 2 (physical offset 0x4000).
    movi r0, 2
    stbi r0, 0xFF01
    movi r4, 0                 ; y
    movi r11, 18               ; board height
    movi r12, 20               ; board width
    movi r13, 17               ; bottom edge
    movi r14, 19               ; right edge
map_row:
    movi r5, 0                 ; x
map_column:
    movi r7, 0                 ; floor tile
    jz r4, map_wall
    je r4, r13, map_wall
    jz r5, map_wall
    je r5, r14, map_wall
    jmp map_store
map_wall:
    movi r7, 1
map_store:
    mul r6, r4, r12
    add r6, r6, r5
    movi r8, 0xC000
    add r6, r6, r8
    stb r6, r7
    add r5, r5, r3
    jlt r5, r12, map_column
    add r4, r4, r3
    jlt r4, r11, map_row

new_game:
    ; Persistent game state.
    movi r20, 4                ; length
    movi r21, 3                ; direction: 0 up, 1 down, 2 left, 3 right
    movi r22, 10               ; head x
    movi r23, 9                ; head y
    movi r24, 15               ; food x
    movi r25, 9                ; food y
    movi r26, 0                ; game-over flag
    movi r27, 0                ; score

    ; Body coordinate arrays: X at 0x0200, Y at 0x0240.
    movi r0, 10
    stbi r0, 0x0200
    movi r0, 9
    stbi r0, 0x0201
    movi r0, 8
    stbi r0, 0x0202
    movi r0, 7
    stbi r0, 0x0203
    movi r0, 9
    stbi r0, 0x0240
    stbi r0, 0x0241
    stbi r0, 0x0242
    stbi r0, 0x0243
    jmp render_frame

game_tick:
    ; Read the held console-button mask from FF22.
    ldbi r0, 0xFF22
    movi r1, 1
    and r2, r0, r1
    jnz r2, request_up
    movi r1, 2
    and r2, r0, r1
    jnz r2, request_down
    movi r1, 4
    and r2, r0, r1
    jnz r2, request_left
    movi r1, 8
    and r2, r0, r1
    jnz r2, request_right
    jmp direction_ready

request_up:
    movi r1, 1
    je r21, r1, direction_ready
    movi r21, 0
    jmp direction_ready
request_down:
    movi r1, 0
    je r21, r1, direction_ready
    movi r21, 1
    jmp direction_ready
request_left:
    movi r1, 3
    je r21, r1, direction_ready
    movi r21, 2
    jmp direction_ready
request_right:
    movi r1, 2
    je r21, r1, direction_ready
    movi r21, 3

direction_ready:
    ; Shift every body coordinate toward the tail.
    movi r3, 1
    mov r4, r20
    movi r0, 0
shift_body:
    sub r5, r4, r3
    movi r6, 0x0200
    add r7, r6, r5
    ldb r8, r7
    add r7, r6, r4
    stb r7, r8
    movi r6, 0x0240
    add r7, r6, r5
    ldb r8, r7
    add r7, r6, r4
    stb r7, r8
    sub r4, r4, r3
    jgt r4, r0, shift_body

    ; Move the head one cell.
    jz r21, move_up
    movi r1, 1
    je r21, r1, move_down
    movi r1, 2
    je r21, r1, move_left
    add r22, r22, r3
    jmp head_moved
move_up:
    sub r23, r23, r3
    jmp head_moved
move_down:
    add r23, r23, r3
    jmp head_moved
move_left:
    sub r22, r22, r3

head_moved:
    stbi r22, 0x0200
    stbi r23, 0x0240

    ; Wall collision.
    jz r22, crashed
    jz r23, crashed
    movi r1, 19
    je r22, r1, crashed
    movi r1, 17
    je r23, r1, crashed

    ; Self collision against body elements 1..length-1.
    movi r4, 1
self_check:
    movi r5, 0x0200
    add r5, r5, r4
    ldb r6, r5
    jne r22, r6, self_next
    movi r5, 0x0240
    add r5, r5, r4
    ldb r6, r5
    je r23, r6, crashed
self_next:
    add r4, r4, r3
    jlt r4, r20, self_check

    ; Eating grows the snake and advances the deterministic food pattern.
    jne r22, r24, render_frame
    jne r23, r25, render_frame
    movi r1, 24
    jge r20, r1, move_food
    add r20, r20, r3
    add r27, r27, r3
move_food:
    movi r1, 7
    add r24, r24, r1
    movi r1, 18
    mod r24, r24, r1
    add r24, r24, r3
    movi r1, 5
    add r25, r25, r1
    movi r1, 16
    mod r25, r25, r1
    add r25, r25, r3
    jmp render_frame

crashed:
    movi r26, 1

render_frame:
    ; Sprite descriptors begin at physical VRAM 0x5000, visible at D000
    ; while bank 2 is selected. Each descriptor is x, y, color, size.
    movi r0, 2
    stbi r0, 0xFF01
    movi r4, 0
    movi r5, 0xD000
    movi r9, 8
    movi r10, 0xDA             ; snake color
    jz r26, render_segment
    movi r10, 0xC2             ; red snake after a crash
render_segment:
    movi r6, 0x0200
    add r6, r6, r4
    ldb r7, r6
    mul r7, r7, r9
    stb r5, r7
    add r5, r5, r3
    movi r8, 0
    stb r5, r8
    add r5, r5, r3
    movi r6, 0x0240
    add r6, r6, r4
    ldb r7, r6
    mul r7, r7, r9
    stb r5, r7
    add r5, r5, r3
    stb r5, r8
    add r5, r5, r3
    stb r5, r10
    add r5, r5, r3
    movi r8, 7
    stb r5, r8
    add r5, r5, r3
    add r4, r4, r3
    jlt r4, r20, render_segment

    ; Food sprite follows the last snake segment.
    mul r7, r24, r9
    stb r5, r7
    add r5, r5, r3
    movi r8, 0
    stb r5, r8
    add r5, r5, r3
    mul r7, r25, r9
    stb r5, r7
    add r5, r5, r3
    stb r5, r8
    add r5, r5, r3
    movi r8, 0xC2
    stb r5, r8
    add r5, r5, r3
    movi r8, 7
    stb r5, r8

    add r0, r20, r3
    stbi r0, 0xFF13
    movi r0, 0
    stbi r0, 0xFF14
    movi r0, 1
    stbi r0, 0xFF12

    jnz r26, game_over_wait

    ; Move once every eight Pocket Color frames. Subtraction intentionally
    ; handles the 16-bit timer wrapping back to zero.
    %frames r4
wait_for_tick:
    %frames r5
    sub r6, r5, r4
    movi r7, 8
    jlt r6, r7, wait_for_tick
    movi r0, 0
    jmp game_tick

game_over_wait:
    ; Start is bit 6 in the held-button register.
    ldbi r0, 0xFF22
    movi r1, 64
    and r2, r0, r1
    jnz r2, new_game
    jmp game_over_wait
