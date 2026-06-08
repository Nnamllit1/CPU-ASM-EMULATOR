; Draw three RGB332 color bars into the first VRAM bank.
start:
    movi r0, 0xE0
    movi r1, 0xC000
    movi r2, 1
    movi r3, 0xC100

draw_red:
    stb r1, r0
    add r1, r1, r2
    jlt r1, r3, draw_red

    movi r0, 0x1C
    movi r3, 0xC200
draw_green:
    stb r1, r0
    add r1, r1, r2
    jlt r1, r3, draw_green

    movi r0, 0x03
    movi r3, 0xC300
draw_blue:
    stb r1, r0
    add r1, r1, r2
    jlt r1, r3, draw_blue

    movi r0, 1
    stbi r0, 0xFF12
    hlt
