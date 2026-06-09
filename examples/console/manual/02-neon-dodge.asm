; Neon Dodge
; Target: Pocket Color
;
; Controls:
;   Left / Right  Move
;   Start         Restart after crash
;
; Goal:
;   Dodge falling neon blocks. Every avoided block increases the score.
;
; Uses:
;   tile mode, sprite descriptors, held buttons, frame timer, cycle timer,
;   RGB332 colors, simple audio.

.reset start
.org 8

; ---------------------------------------------------------------------------
; Boot
; ---------------------------------------------------------------------------

start:
    ldbi r0, 0xFF30
    jz r0, pocket_ok
    %println r0 wrong_profile_text
    hlt

pocket_ok:
    %println r0 boot_text
    call start_audio
    call build_tiles
    call build_background
    call new_game
    call enter_tile_mode
    %frames r28
    mov r29, r28                 ; move delay timer
    jmp main_loop

wrong_profile_text:
    .asciiz "Neon Dodge requires the Pocket Color profile."
boot_text:
    .asciiz "Neon Dodge booting: sprites, tiles, timers, input, audio."

; ---------------------------------------------------------------------------
; Main loop
; ---------------------------------------------------------------------------

main_loop:
    %frames r0
    je r0, r28, main_loop
    mov r28, r0

    ldbi r1, 0x0002              ; game-over flag
    jnz r1, game_over_loop

    call poll_controls
    call update_meteors
    call update_score_bar
    %present r0
    jmp main_loop

game_over_loop:
    call flash_player
    call update_score_bar
    %present r0

wait_restart:
    ldbi r0, 0xFF22
    movi r1, 64                  ; Start button
    and r2, r0, r1
    jnz r2, new_game_and_return
    jmp main_loop

new_game_and_return:
    call new_game
    jmp main_loop

; ---------------------------------------------------------------------------
; RAM layout
; ---------------------------------------------------------------------------
;
; 0x0000  player x, byte
; 0x0001  player y, byte
; 0x0002  game-over flag, byte
; 0x0004  score, 16-bit
;
; Sprite descriptors at VRAM bank 2:
;   0xD000  player
;   0xD006  meteor 1
;   0xD00C  meteor 2
;   ...
;
; Sprite descriptor format:
;   x low, x high, y low, y high, RGB332 color, square size

new_game:
    movi r0, 76
    stbi r0, 0x0000              ; player x
    movi r0, 128
    stbi r0, 0x0001              ; player y
    movi r0, 0
    stbi r0, 0x0002              ; game-over = false
    sti r0, 0x0004               ; score = 0

    call initialize_sprites
    call update_score_bar
    call play_start_sound
    ret

; ---------------------------------------------------------------------------
; Controls
; ---------------------------------------------------------------------------

poll_controls:
    ; Movement is intentionally delayed: one move every 2 frames.
    sub r1, r28, r29
    movi r2, 2
    jlt r1, r2, controls_done
    mov r29, r28

    ldbi r0, 0xFF22

    movi r1, 4                   ; Left
    and r2, r0, r1
    jnz r2, move_left

    movi r1, 8                   ; Right
    and r2, r0, r1
    jnz r2, move_right

    jmp controls_done

move_left:
    ldbi r4, 0x0000
    movi r5, 3
    jlt r4, r5, clamp_left
    sub r4, r4, r5
    stbi r4, 0x0000
    call update_player_sprite
    jmp controls_done

clamp_left:
    movi r4, 0
    stbi r4, 0x0000
    call update_player_sprite
    jmp controls_done

move_right:
    ldbi r4, 0x0000
    movi r5, 152
    jge r4, r5, controls_done
    movi r5, 3
    add r4, r4, r5
    stbi r4, 0x0000
    call update_player_sprite

controls_done:
    ret

; ---------------------------------------------------------------------------
; Tile graphics
; ---------------------------------------------------------------------------

build_tiles:
    ; Build four solid-ish tiles:
    ;   0 = dark background
    ;   1 = dark grid
    ;   2 = score bar
    ;   3 = danger / crash
    movi r0, 0
    stbi r0, 0xFF01              ; VRAM bank 0
    stbi r0, 0xFF02              ; ROM bank 0

    movi r1, 0                   ; tile index
    movi r2, 4                   ; tile count

tile_loop:
    movi r3, tile_colors
    add r3, r3, r1
    ldbr r4, r3                  ; color
    movi r5, 64
    mul r6, r1, r5
    movi r7, 0xC000
    add r6, r6, r7               ; tile address
    movi r8, 0

tile_pixel_loop:
    mov r9, r4

    ; Add one dark vertical edge so tiles look less flat.
    movi r10, 8
    mod r11, r8, r10
    jz r11, tile_edge
    jmp tile_store

tile_edge:
    movi r9, 0

tile_store:
    stb r6, r9
    movi r10, 1
    add r6, r6, r10
    add r8, r8, r10
    jlt r8, r5, tile_pixel_loop

    add r1, r1, r10
    jlt r1, r2, tile_loop
    ret

build_background:
    ; Tile map lives in VRAM bank 2 at C000.
    ; Fill 20x18 map. Last row is the score bar area.
    movi r0, 2
    stbi r0, 0xFF01

    movi r1, 0xC000
    movi r2, 0xC168              ; 360 bytes = 20*18
    movi r3, 1
    movi r4, 0

background_loop:
    stb r1, r4
    add r1, r1, r3
    jlt r1, r2, background_loop

    ; Draw bottom HUD row with grid tile.
    movi r1, 0xC154              ; row 17 start = 17*20
    movi r2, 0xC168
    movi r4, 1

hud_row_loop:
    stb r1, r4
    add r1, r1, r3
    jlt r1, r2, hud_row_loop
    ret

enter_tile_mode:
    movi r0, 3                   ; display enabled + tile mode
    stbi r0, 0xFF10

    movi r0, 10                  ; 1 player + 9 meteors
    stbi r0, 0xFF13
    movi r0, 0
    stbi r0, 0xFF14

    %present r0
    ret

; ---------------------------------------------------------------------------
; Sprites
; ---------------------------------------------------------------------------

initialize_sprites:
    movi r0, 2
    stbi r0, 0xFF01

    ; Player sprite at D000.
    call update_player_sprite
    movi r0, 0xDA
    stbi r0, 0xD004              ; player color
    movi r0, 7
    stbi r0, 0xD005              ; 8x8-ish square

    ; Meteor sprites.
    movi r1, 1                   ; meteor index 1..9
    movi r2, 0xD006              ; descriptor pointer
    movi r3, 10                  ; stop at 10
    movi r4, 6                   ; descriptor size

meteor_init_loop:
    call reset_meteor_at_pointer
    add r2, r2, r4
    movi r5, 1
    add r1, r1, r5
    jlt r1, r3, meteor_init_loop
    ret

update_player_sprite:
    movi r0, 2
    stbi r0, 0xFF01

    ldbi r1, 0x0000
    stbi r1, 0xD000
    movi r2, 0
    stbi r2, 0xD001

    ldbi r1, 0x0001
    stbi r1, 0xD002
    stbi r2, 0xD003
    ret

reset_meteor_at_pointer:
    ; Input:
    ;   r1 = meteor index
    ;   r2 = sprite descriptor pointer
    ;
    ; x = (cycles + index*37) mod 152
    ; y = (index*13) mod 80

    %cycles r6
    movi r7, 37
    mul r8, r1, r7
    add r6, r6, r8
    movi r7, 152
    mod r6, r6, r7
    stb r2, r6                  ; x low

    movi r7, 1
    add r9, r2, r7
    movi r6, 0
    stb r9, r6                  ; x high

    movi r7, 13
    mul r6, r1, r7
    movi r7, 80
    mod r6, r6, r7
    movi r7, 2
    add r9, r2, r7
    stb r9, r6                  ; y low

    movi r7, 3
    add r9, r2, r7
    movi r6, 0
    stb r9, r6                  ; y high

    movi r7, 4
    add r9, r2, r7
    movi r6, 0xC2
    stb r9, r6                  ; meteor color

    movi r7, 5
    add r9, r2, r7
    movi r6, 7
    stb r9, r6                  ; size
    ret

; ---------------------------------------------------------------------------
; Meteor update and collision
; ---------------------------------------------------------------------------

update_meteors:
    movi r0, 2
    stbi r0, 0xFF01

    movi r1, 1                   ; meteor index
    movi r2, 0xD006              ; descriptor pointer
    movi r3, 10                  ; stop
    movi r4, 6                   ; descriptor size

meteor_loop:
    call update_one_meteor

    add r2, r2, r4
    movi r5, 1
    add r1, r1, r5
    jlt r1, r3, meteor_loop
    ret

update_one_meteor:
    ; y pointer = descriptor + 2
    movi r5, 2
    add r6, r2, r5
    ldb r7, r6                   ; y

    ; speed = 1 + ((index + score) mod 3)
    ldi r8, 0x0004
    add r8, r8, r1
    movi r9, 3
    mod r8, r8, r9
    movi r9, 1
    add r8, r8, r9

    add r7, r7, r8
    movi r9, 144
    jlt r7, r9, meteor_store_y

    ; Wrapped past bottom: respawn at top and increase score.
    movi r7, 0
    stb r6, r7
    call reset_meteor_x_only

    ldi r10, 0x0004
    movi r11, 1
    add r10, r10, r11
    sti r10, 0x0004
    call play_score_sound
    ret

meteor_store_y:
    stb r6, r7
    call check_collision
    ret

reset_meteor_x_only:
    %cycles r6
    movi r7, 41
    mul r8, r1, r7
    add r6, r6, r8
    movi r7, 152
    mod r6, r6, r7
    stb r2, r6                  ; x low

    movi r7, 1
    add r8, r2, r7
    movi r6, 0
    stb r8, r6                  ; x high
    ret

check_collision:
    ; Only check near bottom.
    ; r2 = descriptor pointer.
    ;
    ; if meteor_y < 122: no collision
    ; if meteor_x + 7 < player_x: no collision
    ; if player_x + 7 < meteor_x: no collision
    ; else crash

    movi r5, 2
    add r6, r2, r5
    ldb r7, r6                   ; meteor y

    movi r8, 122
    jlt r7, r8, collision_done

    ldb r9, r2                   ; meteor x
    ldbi r10, 0x0000             ; player x

    movi r11, 7
    add r12, r9, r11             ; meteor right
    jlt r12, r10, collision_done

    add r13, r10, r11            ; player right
    jlt r13, r9, collision_done

    call crash

collision_done:
    ret

crash:
    movi r0, 1
    stbi r0, 0x0002

    ; Player becomes red.
    movi r0, 0xC2
    stbi r0, 0xD004

    call play_crash_sound
    ret

flash_player:
    ; Flicker player color after crash using frame timer.
    %frames r0
    movi r1, 4
    div r0, r0, r1
    movi r1, 2
    mod r0, r0, r1

    movi r2, 0xC2
    jz r0, flash_store
    movi r2, 0xDA

flash_store:
    stbi r2, 0xD004
    ret

; ---------------------------------------------------------------------------
; Score HUD
; ---------------------------------------------------------------------------

update_score_bar:
    ; Bottom row: score modulo 20 as a neon bar.
    movi r0, 2
    stbi r0, 0xFF01

    ; Clear HUD row to tile 1.
    movi r1, 0xC154
    movi r2, 0xC168
    movi r3, 1
    movi r4, 1

score_clear_loop:
    stb r1, r4
    add r1, r1, r3
    jlt r1, r2, score_clear_loop

    ; Fill score amount with tile 2.
    ldi r5, 0x0004
    movi r6, 20
    mod r5, r5, r6
    jz r5, score_done

    movi r1, 0xC154
    movi r7, 0
    movi r8, 2

score_fill_loop:
    stb r1, r8
    add r1, r1, r3
    add r7, r7, r3
    jlt r7, r5, score_fill_loop

score_done:
    ret

; ---------------------------------------------------------------------------
; Audio
; ---------------------------------------------------------------------------

start_audio:
    %tone r0 0 55 0 16
    %tone r0 1 110 0 10
    %tone r0 2 165 0 8
    %tone r0 3 220 0 0
    ret

play_start_sound:
    movi r0, 3
    stbi r0, 0xFF40
    movi r0, 184
    stbi r0, 0xFF42
    movi r0, 1
    stbi r0, 0xFF43
    movi r0, 48
    stbi r0, 0xFF44
    ret

play_score_sound:
    movi r0, 3
    stbi r0, 0xFF40
    movi r0, 250
    stbi r0, 0xFF42
    movi r0, 1
    stbi r0, 0xFF43
    movi r0, 64
    stbi r0, 0xFF44
    ret

play_crash_sound:
    movi r0, 3
    stbi r0, 0xFF40
    movi r0, 45
    stbi r0, 0xFF42
    movi r0, 0
    stbi r0, 0xFF43
    movi r0, 120
    stbi r0, 0xFF44
    ret

; ---------------------------------------------------------------------------
; ROM bank 0 data
; ---------------------------------------------------------------------------

.rombank 0

tile_colors:
    .byte 0x00                  ; background
    .byte 0x08                  ; grid / HUD empty
    .byte 0xDA                  ; score bar
    .byte 0xC2                  ; danger
