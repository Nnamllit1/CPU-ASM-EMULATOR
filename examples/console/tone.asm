; Enable audio channel 0 at 440 Hz (0x01B8) and wait for a key.
start:
    %tone r0 0 0xB8 0x01 192

wait:
    movi r1, 0
    in r1
    jz r1, wait

    %toneoff r0 0
    hlt
