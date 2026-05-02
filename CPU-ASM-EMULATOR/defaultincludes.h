#pragma once

inline constexpr const char* DEFAULT_INCLUDES_ASM = R"asm(
; Default macros compiled into the emulator executable.
; These definitions are prepended before the user's assembly is parsed.

%macro putc reg value
    movi {reg}, {value}
    out {reg}
%endmacro

%macro putn reg value
    movi {reg}, {value}
    outn {reg}
%endmacro

%macro space reg
    movi {reg}, 32
    out {reg}
%endmacro

%macro newline reg
    movi {reg}, 10
    out {reg}
%endmacro

%macro print reg label
    movi {reg}, {label}
    outs {reg}
%endmacro

%macro println reg label
    movi {reg}, {label}
    outs {reg}
    movi {reg}, 10
    out {reg}
%endmacro

%macro read reg
    in {reg}
%endmacro

%macro readkey reg
    inkey {reg}
%endmacro
)asm";
