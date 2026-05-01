#pragma once

inline constexpr const char* DEFAULT_INCLUDES_ASM = R"asm(
; Default macros compiled into the emulator executable.
; These definitions are prepended before the user's assembly is parsed.

%macro putc reg value
    movi {reg}, {value}
    out {reg}
%endmacro

%macro newline reg
    movi {reg}, 10
    out {reg}
%endmacro
)asm";
