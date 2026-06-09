; Chunk Raytracer
; Target: Pocket Color
;
; Not a game.
; Renders a simple orthographic raytraced scene:
;   - shaded sphere
;   - sky gradient
;   - checker floor
;   - fake elliptical shadow
;
; Chunk-based:
;   The image is rendered in 8x8 chunks.
;   During rendering, the display is presented after every chunk.
;   After rendering is complete, the program does NOT keep presenting.
;
; Controls:
;   none

.reset start
.org 8

start:
    ldbi r0, 0xFF30
    jz r0, pocket_ok
    %println r0 wrong_profile_text
    hlt

pocket_ok:
    %println r0 boot_text
    call enter_bitmap_mode
    call clear_framebuffer
    %println r0 render_text
    call render_scene

done:
    ; IMPORTANT:
    ; Do not call %present here.
    ; Writing FF12 repeatedly restarts scanout at the top, so the display
    ; would stay stuck near scanline 1.
    jmp done

wrong_profile_text:
    .asciiz "Chunk Raytracer requires the Pocket Color profile."
boot_text:
    .asciiz "Chunk Raytracer booting."
render_text:
    .asciiz "Rendering 8x8 chunks: sphere, floor, shadow."

; ---------------------------------------------------------------------------
; RAM layout
; ---------------------------------------------------------------------------
;
; 0x0030  chunk y
; 0x0031  chunk x
; 0x0032  local y inside chunk
; 0x0033  local x inside chunk
; 0x0034  current pixel x
; 0x0035  current pixel y
;
; Framebuffer:
;   160 * 144 bytes, RGB332, linear bitmap mode.
;
; VRAM:
;   C000-DFFF is an 8 KiB banked VRAM window.
;   FF01 selects VRAM bank.

; ---------------------------------------------------------------------------
; Display setup
; ---------------------------------------------------------------------------

enter_bitmap_mode:
    ; FF10:
    ;   bit 0 = display enabled
    ;   bit 1 = tile mode
    ;
    ; Bitmap mode = bit 0 only.
    movi r0, 1
    stbi r0, 0xFF10
    %present r0
    ret

clear_framebuffer:
    ; Clear VRAM banks 0, 1, 2.
    ; 160*144 = 23040 bytes.
    ; 3 banks * 8192 = 24576 bytes, enough for the framebuffer.

    movi r20, 0                  ; current VRAM bank
    movi r21, 3                  ; bank count
    movi r22, 1

clear_bank_loop:
    stbi r20, 0xFF01

    movi r0, 0xC000
    movi r1, 0xE000
    movi r2, 0x00

clear_byte_loop:
    stb r0, r2
    add r0, r0, r22
    jlt r0, r1, clear_byte_loop

    add r20, r20, r22
    jlt r20, r21, clear_bank_loop

    %present r0
    ret

; ---------------------------------------------------------------------------
; Chunk renderer
; ---------------------------------------------------------------------------

render_scene:
    ; chunk_y = 0
    movi r0, 0
    stbi r0, 0x0030

chunk_y_loop:
    ; chunk_x = 0
    movi r0, 0
    stbi r0, 0x0031

chunk_x_loop:
    call render_chunk

    ; Present after every 8x8 chunk so rendering progress is visible.
    ; This is safe because render_chunk does enough work between presents.
    %present r0

    ; chunk_x += 8
    ldbi r0, 0x0031
    movi r1, 8
    add r0, r0, r1
    stbi r0, 0x0031

    movi r2, 160
    jlt r0, r2, chunk_x_loop

    ; chunk_y += 8
    ldbi r0, 0x0030
    movi r1, 8
    add r0, r0, r1
    stbi r0, 0x0030

    movi r2, 144
    jlt r0, r2, chunk_y_loop

    ; One final present after the whole image is complete.
    ; After this, done: only idles.
    %present r0
    ret

render_chunk:
    ; local_y = 0
    movi r0, 0
    stbi r0, 0x0032

local_y_loop:
    ; local_x = 0
    movi r0, 0
    stbi r0, 0x0033

local_x_loop:
    ; pixel_x = chunk_x + local_x
    ldbi r0, 0x0031
    ldbi r1, 0x0033
    add r0, r0, r1
    stbi r0, 0x0034

    ; pixel_y = chunk_y + local_y
    ldbi r0, 0x0030
    ldbi r1, 0x0032
    add r0, r0, r1
    stbi r0, 0x0035

    call shade_pixel
    call write_current_pixel

    ; local_x++
    ldbi r0, 0x0033
    movi r1, 1
    add r0, r0, r1
    stbi r0, 0x0033

    movi r2, 8
    jlt r0, r2, local_x_loop

    ; local_y++
    ldbi r0, 0x0032
    movi r1, 1
    add r0, r0, r1
    stbi r0, 0x0032

    movi r2, 8
    jlt r0, r2, local_y_loop

    ret

; ---------------------------------------------------------------------------
; Pixel shader / raytracer
; ---------------------------------------------------------------------------

shade_pixel:
    ; Output:
    ;   r10 = RGB332 color
    ;
    ; Scene:
    ;   Orthographic camera.
    ;   Sphere centered at screen-space (80, 64), radius 42.
    ;
    ; Hit test:
    ;   dx = abs(x - 80)
    ;   dy = abs(y - 64)
    ;   dist2 = dx*dx + dy*dy
    ;   if dist2 < radius^2 => sphere hit
    ;   else background/floor

    ldbi r21, 0x0034             ; x
    ldbi r20, 0x0035             ; y

    ; dx = abs(x - 80)
    movi r0, 80
    jlt r21, r0, sphere_x_left
    sub r11, r21, r0
    jmp sphere_x_done

sphere_x_left:
    sub r11, r0, r21

sphere_x_done:
    ; dy = abs(y - 64)
    movi r0, 64
    jlt r20, r0, sphere_y_up
    sub r12, r20, r0
    jmp sphere_y_done

sphere_y_up:
    sub r12, r0, r20

sphere_y_done:
    ; dist2 = dx*dx + dy*dy
    mul r13, r11, r11
    mul r0, r12, r12
    add r13, r13, r0

    ; radius^2 = 42*42 = 1764
    movi r0, 1764
    jlt r13, r0, sphere_hit

    call shade_background
    ret

sphere_hit:
    ; shade = (radius^2 - dist2) / 160
    ; More center = brighter.
    movi r0, 1764
    sub r15, r0, r13

    movi r0, 160
    div r1, r15, r0

    ; Light bias: upper-left is brighter.
    ldbi r2, 0x0034              ; x
    movi r0, 80
    jlt r2, r0, sphere_light_x
    jmp sphere_light_y

sphere_light_x:
    movi r0, 1
    add r1, r1, r0

sphere_light_y:
    ldbi r2, 0x0035              ; y
    movi r0, 64
    jlt r2, r0, sphere_light_add_y
    jmp sphere_light_done

sphere_light_add_y:
    movi r0, 1
    add r1, r1, r0

sphere_light_done:
    ; Clamp shade to max palette index 7.
    movi r0, 7
    jlt r1, r0, sphere_color_map
    movi r1, 7

sphere_color_map:
    jz r1, sphere_c0
    movi r0, 1
    je r1, r0, sphere_c1
    movi r0, 2
    je r1, r0, sphere_c2
    movi r0, 3
    je r1, r0, sphere_c3
    movi r0, 4
    je r1, r0, sphere_c4
    movi r0, 5
    je r1, r0, sphere_c5
    movi r0, 6
    je r1, r0, sphere_c6

sphere_c7:
    movi r10, 0xFF               ; bright highlight
    ret

sphere_c0:
    movi r10, 0x03               ; dark blue edge
    ret

sphere_c1:
    movi r10, 0x08               ; blue/green shadow
    ret

sphere_c2:
    movi r10, 0x12               ; cyan
    ret

sphere_c3:
    movi r10, 0x1A               ; bright cyan/green
    ret

sphere_c4:
    movi r10, 0x49               ; purple midtone
    ret

sphere_c5:
    movi r10, 0xDA               ; yellow
    ret

sphere_c6:
    movi r10, 0xFF               ; white-ish
    ret

; ---------------------------------------------------------------------------
; Background: sky, floor, shadow
; ---------------------------------------------------------------------------

shade_background:
    ldbi r20, 0x0035             ; y

    ; Horizon at y = 82.
    movi r0, 82
    jlt r20, r0, bg_sky

    jmp bg_floor

bg_sky:
    ; Sky gradient by y / 16.
    movi r0, 16
    div r1, r20, r0

    jz r1, sky_0
    movi r0, 1
    je r1, r0, sky_1
    movi r0, 2
    je r1, r0, sky_2
    movi r0, 3
    je r1, r0, sky_3

sky_4:
    movi r10, 0x1A               ; near horizon
    ret

sky_0:
    movi r10, 0x03               ; top blue
    ret

sky_1:
    movi r10, 0x08
    ret

sky_2:
    movi r10, 0x12
    ret

sky_3:
    movi r10, 0x1A
    ret

bg_floor:
    ; Fake elliptical shadow centered under the sphere.
    ; shadow center = (80, 112)
    ; if dx*dx + dy*dy*4 < 2200 => shadow

    ldbi r21, 0x0034             ; x
    ldbi r20, 0x0035             ; y

    ; dx = abs(x - 80)
    movi r0, 80
    jlt r21, r0, shadow_x_left
    sub r11, r21, r0
    jmp shadow_x_done

shadow_x_left:
    sub r11, r0, r21

shadow_x_done:
    ; dy = abs(y - 112)
    movi r0, 112
    jlt r20, r0, shadow_y_up
    sub r12, r20, r0
    jmp shadow_y_done

shadow_y_up:
    sub r12, r0, r20

shadow_y_done:
    ; d = dx*dx + dy*dy*4
    mul r13, r11, r11
    mul r14, r12, r12
    movi r0, 4
    mul r14, r14, r0
    add r13, r13, r14

    movi r0, 2200
    jlt r13, r0, floor_shadow

    ; Checker floor:
    ; checker = (x/8 + y/8) % 2
    ldbi r21, 0x0034
    ldbi r20, 0x0035

    movi r0, 8
    div r1, r21, r0
    div r2, r20, r0
    add r1, r1, r2

    movi r0, 2
    mod r1, r1, r0

    jz r1, floor_dark

floor_bright:
    movi r10, 0x49               ; purple/blue tile
    ret

floor_dark:
    movi r10, 0x08               ; dark tile
    ret

floor_shadow:
    movi r10, 0x00               ; shadow
    ret

; ---------------------------------------------------------------------------
; Framebuffer write
; ---------------------------------------------------------------------------

write_current_pixel:
    ; Reads:
    ;   0x0034 = x
    ;   0x0035 = y
    ;   r10    = RGB332 color
    ;
    ; offset = y * 160 + x
    ; bank   = offset / 8192
    ; addr   = C000 + (offset % 8192)

    ldbi r20, 0x0035             ; y
    ldbi r21, 0x0034             ; x

    movi r0, 160
    mul r1, r20, r0
    add r1, r1, r21

    movi r0, 8192
    div r2, r1, r0               ; VRAM bank
    mod r3, r1, r0               ; offset in bank

    stbi r2, 0xFF01

    movi r0, 0xC000
    add r3, r3, r0

    stb r3, r10
    ret
