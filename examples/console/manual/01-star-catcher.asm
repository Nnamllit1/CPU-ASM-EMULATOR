; Star Catcher
; Target: Pocket Color
;
; Controls:
;   Left / Right  Move catcher
;   A             speed boost while held
;
; Gameplay:
;   Catch the falling star. No game over, just score.
;
; Hardware:
;   tile mode, sprites, frame timer, cycle timer, held buttons, audio registers.

.reset start
.org 8

start:
    ldbi r0, 0xFF30
    jz r0, pocket_ok
    %println r0 wrong_profile_text
    hlt

pocket_ok:
    %println r0 boot_text
    call build_tiles
    call build_tilemap
    call new_game
    call enter_tile_mode
    %frames r28
    mov r29, r28                 ; movement timer
    jmp main_loop

wrong_profile_text:
    .asciiz "Star Catcher requires the Pocket Color profile."
boot_text:
    .asciiz "Star Catcher booting."

; ---------------------------------------------------------------------------
; RAM layout
; ---------------------------------------------------------------------------
;
; 0x0000  catcher x, byte
; 0x0001  star x, byte
; 0x0002  star y, byte
; 0x0004  score, 16-bit
;
; Sprite descriptors in VRAM bank 2:
;   D000  catcher
;   D006  falling star
;
; Descriptor:
;   x low, x high, y low, y high, RGB332 color, size

new_game:
    movi r0, 76
    stbi r0, 0x0000              ; catcher x

    movi r0, 50
    stbi r0, 0x0001              ; star x

    movi r0, 0
    stbi r0, 0x0002              ; star y
    sti r0, 0x0004               ; score

    call init_sprites
    call update_hud
    call play_start_sound
    ret

; ---------------------------------------------------------------------------
; Main loop
; ---------------------------------------------------------------------------

main_loop:
    %frames r0
    je r0, r28, main_loop
    mov r28, r0

    call poll_controls
    call update_star
    call update_hud
    %present r0

    jmp main_loop

; ---------------------------------------------------------------------------
; Controls
; ---------------------------------------------------------------------------

poll_controls:
    ; Move once every 2 frames, or every frame while A is held.
    ldbi r0, 0xFF22

    movi r1, 16                  ; A button
    and r2, r0, r1
    jnz r2, movement_allowed

    sub r1, r28, r29
    movi r2, 2
    jlt r1, r2, controls_done

movement_allowed:
    mov r29, r28

    movi r1, 4                   ; Left
    and r2, r0, r1
    jnz r2, move_left

    movi r1, 8                   ; Right
    and r2, r0, r1
    jnz r2, move_right

    jmp controls_done

move_left:
    ldbi r3, 0x0000
    movi r4, 4
    jlt r3, r4, set_left_edge
    sub r3, r3, r4
    stbi r3, 0x0000
    call update_catcher_sprite
    jmp controls_done

set_left_edge:
    movi r3, 0
    stbi r3, 0x0000
    call update_catcher_sprite
    jmp controls_done

move_right:
    ldbi r3, 0x0000
    movi r4, 148
    jge r3, r4, controls_done
    movi r4, 4
    add r3, r3, r4
    stbi r3, 0x0000
    call update_catcher_sprite

controls_done:
    ret

; ---------------------------------------------------------------------------
; Star logic
; ---------------------------------------------------------------------------

update_star:
    ldbi r0, 0x0002              ; star y

    ; Falling speed: 1 + score % 3.
    ldi r1, 0x0004
    movi r2, 3
    mod r1, r1, r2
    movi r2, 1
    add r1, r1, r2

    add r0, r0, r1
    stbi r0, 0x0002

    movi r2, 120
    jlt r0, r2, star_not_near_catcher

    call check_catch

star_not_near_catcher:
    movi r2, 144
    jlt r0, r2, update_star_sprite

    ; Missed. Respawn without penalty.
    call respawn_star
    call play_miss_sound
    ret

update_star_sprite:
    call update_star_sprite_only
    ret

check_catch:
    ; Catch when:
    ;   star_y >= 120
    ;   star_x overlaps catcher_x..catcher_x+16
    ldbi r3, 0x0001              ; star x
    ldbi r4, 0x0000              ; catcher x

    movi r5, 7
    add r6, r3, r5               ; star right
    jlt r6, r4, catch_done

    movi r5, 16
    add r6, r4, r5               ; catcher right
    jlt r6, r3, catch_done

    ; Caught.
    ldi r7, 0x0004
    movi r8, 1
    add r7, r7, r8
    sti r7, 0x0004
    call play_catch_sound
    call respawn_star

catch_done:
    ret

respawn_star:
    ; x = (cycles + score * 19) mod 152
    %cycles r0
    ldi r1, 0x0004
    movi r2, 19
    mul r1, r1, r2
    add r0, r0, r1
    movi r2, 152
    mod r0, r0, r2
    stbi r0, 0x0001

    movi r0, 0
    stbi r0, 0x0002

    call update_star_sprite_only
    ret

; ---------------------------------------------------------------------------
; Tiles and tilemap
; ---------------------------------------------------------------------------

build_tiles:
    ; VRAM bank 0:
    ; tile 0 = dark background
    ; tile 1 = bottom HUD stripe
    ; tile 2 = score fill
    movi r0, 0
    stbi r0, 0xFF01

    ; Tile 0 at C000-C03F, color 0x00.
    movi r1, 0xC000
    movi r2, 0xC040
    movi r3, 0x00
    call fill_tile_range

    ; Tile 1 at C040-C07F, color 0x08.
    movi r1, 0xC040
    movi r2, 0xC080
    movi r3, 0x08
    call fill_tile_range

    ; Tile 2 at C080-C0BF, color 0xDA.
    movi r1, 0xC080
    movi r2, 0xC0C0
    movi r3, 0xDA
    call fill_tile_range

    ret

fill_tile_range:
    ; r1 = start
    ; r2 = end
    ; r3 = color
    movi r4, 1
fill_tile_loop:
    stb r1, r3
    add r1, r1, r4
    jlt r1, r2, fill_tile_loop
    ret

build_tilemap:
    ; VRAM bank 2 tile map at C000.
    movi r0, 2
    stbi r0, 0xFF01

    ; Fill full 20x18 map with tile 0.
    movi r1, 0xC000
    movi r2, 0xC168
    movi r3, 0
    movi r4, 1

map_clear_loop:
    stb r1, r3
    add r1, r1, r4
    jlt r1, r2, map_clear_loop

    ; Bottom row gets tile 1.
    movi r1, 0xC154
    movi r2, 0xC168
    movi r3, 1

bottom_row_loop:
    stb r1, r3
    add r1, r1, r4
    jlt r1, r2, bottom_row_loop

    ret

enter_tile_mode:
    movi r0, 3
    stbi r0, 0xFF10              ; display + tile mode

    movi r0, 2
    stbi r0, 0xFF13              ; 2 sprites
    movi r0, 0
    stbi r0, 0xFF14

    %present r0
    ret

; ---------------------------------------------------------------------------
; Sprite setup
; ---------------------------------------------------------------------------

init_sprites:
    movi r0, 2
    stbi r0, 0xFF01

    call update_catcher_sprite

    ; Catcher color and size.
    movi r0, 0x1A
    stbi r0, 0xD004
    movi r0, 15
    stbi r0, 0xD005

    call update_star_sprite_only

    ; Star color and size.
    movi r0, 0xDA
    stbi r0, 0xD00A
    movi r0, 7
    stbi r0, 0xD00B

    ret

update_catcher_sprite:
    movi r0, 2
    stbi r0, 0xFF01

    ldbi r1, 0x0000
    stbi r1, 0xD000
    movi r2, 0
    stbi r2, 0xD001

    movi r1, 128
    stbi r1, 0xD002
    stbi r2, 0xD003

    ret

update_star_sprite_only:
    movi r0, 2
    stbi r0, 0xFF01

    ldbi r1, 0x0001
    stbi r1, 0xD006
    movi r2, 0
    stbi r2, 0xD007

    ldbi r1, 0x0002
    stbi r1, 0xD008
    stbi r2, 0xD009

    ret

; ---------------------------------------------------------------------------
; HUD / score bar
; ---------------------------------------------------------------------------

update_hud:
    ; Bottom row shows score modulo 20.
    movi r0, 2
    stbi r0, 0xFF01

    ; Clear bottom row to tile 1.
    movi r1, 0xC154
    movi r2, 0xC168
    movi r3, 1
    movi r4, 1

hud_clear_loop:
    stb r1, r3
    add r1, r1, r4
    jlt r1, r2, hud_clear_loop

    ; Fill with tile 2 according to score.
    ldi r5, 0x0004
    movi r6, 20
    mod r5, r5, r6
    jz r5, hud_done

    movi r1, 0xC154
    movi r7, 0
    movi r8, 2

hud_fill_loop:
    stb r1, r8
    add r1, r1, r4
    add r7, r7, r4
    jlt r7, r5, hud_fill_loop

hud_done:
    ret

; ---------------------------------------------------------------------------
; Audio
; ---------------------------------------------------------------------------

play_start_sound:
    movi r0, 0
    stbi r0, 0xFF40              ; channel 0
    movi r0, 1
    stbi r0, 0xFF41              ; enable
    movi r0, 180
    stbi r0, 0xFF42
    movi r0, 1
    stbi r0, 0xFF43
    movi r0, 50
    stbi r0, 0xFF44
    ret

play_catch_sound:
    movi r0, 1
    stbi r0, 0xFF40              ; channel 1
    stbi r0, 0xFF41              ; enable
    movi r0, 70
    stbi r0, 0xFF42
    movi r0, 2
    stbi r0, 0xFF43
    movi r0, 80
    stbi r0, 0xFF44
    ret

play_miss_sound:
    movi r0, 2
    stbi r0, 0xFF40              ; channel 2
    movi r0, 1
    stbi r0, 0xFF41              ; enable
    movi r0, 80
    stbi r0, 0xFF42
    movi r0, 0
    stbi r0, 0xFF43
    movi r0, 25
    stbi r0, 0xFF44
    ret
    