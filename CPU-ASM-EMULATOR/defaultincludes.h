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

%macro rambank reg bank
    movi {reg}, {bank}
    stbi {reg}, 0xFF00
%endmacro

%macro vbank reg bank
    movi {reg}, {bank}
    stbi {reg}, 0xFF01
%endmacro

%macro rombank reg bank
    movi {reg}, {bank}
    stbi {reg}, 0xFF02
%endmacro

%macro ppumode reg mode
    movi {reg}, {mode}
    stbi {reg}, 0xFF10
%endmacro

%macro present reg
    movi {reg}, 1
    stbi {reg}, 0xFF12
%endmacro

%macro sprites reg count
    movi {reg}, {count}
    stbi {reg}, 0xFF13
%endmacro
)asm";
