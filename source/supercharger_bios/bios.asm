;;   ==========================================================================
;;   This file is part of 6502.ts, an emulator for 6502 based systems built
;;   in Typescript.
;;
;;   Copyright (C) 2014 - 2018 Christian Speckner & contributors
;;
;;   This program is free software; you can redistribute it and/or modify
;;   it under the terms of the GNU General Public License as published by
;;   the Free Software Foundation; either version 2 of the License, or
;;   (at your option) any later version.
;;
;;   This program is distributed in the hope that it will be useful,
;;   but WITHOUT ANY WARRANTY; without even the implied warranty of
;;   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;;   GNU General Public License for more details.
;;
;;   You should have received a copy of the GNU General Public License along
;;   with this program; if not, write to the Free Software Foundation, Inc.,
;;   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
;;   ==========================================================================
;;   This is the source for the stub BIOS used in supercharger emulation. It is
;;   a heavily modified version of the BIOS included in Stella, which in turn
;;   is based on code developed by Eckard Stolberg.
;;   ==========================================================================
;;   Interface:
;;
;;   * The BIOS code should be relocated to the beginning of the ROM bank
;;   * The entry vector ($F807) must be patched into the last four bytes of the
;;     ROM bank
;;   * A canary value of zero must be placed at $FFFB
;;   * $FFFA encodes frame rate and palette
;;         * bit 0: 0 = NTSC/PAL60 , 1 = PAL50
;;         * bit 1: 0 = NTSC, 1 = PAL
;;   * The BIOS triggers the actual load by writing the requested multiload ID
;;     $FFF9. The write originates from page zero.
;;   * After loading, the following values must be patched into the ROM:
;;       - $FFF0: control word
;;       - $FFF1: a random value for A
;;       - $FFF2 -- $FFF3: entry point
;;   ==========================================================================
        processor 6502

VSYNC   equ  $00
VBLANK  equ  $01
WSYNC   equ  $02
COLUPF  equ  $08
COLUBK  equ  $09
CTRLPF  equ  $0a
PF0     equ  $0d
PF1     equ  $0e
PF2     equ  $0f
RESP0   equ  $10
RESP1   equ  $11
AUDC0   equ  $15
AUDF0   equ  $17
AUDV0   equ  $19
AUDV1   equ  $1a
GRP0    equ  $1b
GRP1    equ  $1c
ENAM0   equ  $1d
ENAM1   equ  $1e
ENABL   equ  $1f
HMP0    equ  $20
HMP1    equ  $21
HMOVE   equ  $2a

        SEG code
; ===
; Entry point for multi-load reading
; ===
        org $F800

; grap the requested load ID and store it away at $80...
        LDA $FA
        STA $80
; ... then jump to the load routing
        JMP load

; ===
; System reset
; ===

        org $F807
start:
        SEI
        CLD

        LDA #0
        LDX #$FF
        TXS
        TAX
        TAY

clearmem:
; the regular init dance
        STA $00,X
        INX
        BNE clearmem

        JMP load

load:
; Configure banking and enable RAM writes
        LDX #0
        LDA $F006
        STA $FFF8

; Clear some of the 2600's RAM and TIA registers like the real SC BIOS does
        LDY #$00
        LDX #$28
mlclr1:
        STY $04,X
        DEX
        BPL mlclr1

        LDX #$1C
mlclr2:
        STY $81,X
        DEX
        BPL mlclr2

; emulated loads bars
startbars:
        LDA #$00
        STA GRP0
        STA GRP1
        STA ENAM0
        STA ENAM1
        STA ENABL
        STA AUDV0
        STA AUDV1
        STA COLUPF
        STA VBLANK

        LDA #$10
        STA HMP1
        STA WSYNC
        LDX #$07
        DEX
pos:
        DEX
        BNE pos
        LDA #$00
        STA HMP0
        STA RESP0
        STA RESP1
        STA WSYNC
        STA HMOVE

        LDA #%00000101
        STA CTRLPF
        LDA #$FF
        STA PF0
        STA PF1
        STA PF2
        STA $84
        STA $85
        LDA #$F0
        STA $83

        LDX #$74
        LDA #$02
        BIT $FFFA
        BEQ tiantsc
tiapal:
        LDX #$D4
tiantsc:
        STX COLUBK

        LDA #$0C
        STA AUDC0
        LDA #$1F
        STA AUDF0
        STA $82
        LDA #$07
        STA AUDV0
a1:
        LDX #$08
        LDY #$00
a2:
        STA WSYNC
        DEY
        BNE a2

        LDA #$01
        BIT $FFFA
        BEQ tvntsc
        LDY #50
tvpal:
        STA WSYNC
        DEY
        BNE tvpal
tvntsc:

        STA WSYNC
        STA WSYNC
        LDA #$02
        STA WSYNC
        STA VSYNC
        STA WSYNC
        STA WSYNC
        STA WSYNC
        LDA #$00
        STA VSYNC
        DEX
        BPL a2
        ASL $83
        ROR $84
        ROL $85
        LDA $83
        STA PF0
        LDA $84
        STA PF1
        LDA $85
        STA PF2
        LDX $82
        DEX
        STX $82
        STX AUDF0
        CPX #$0A
        BNE a1

        LDA #%00000010
        STA VBLANK

        LDX #$1C
        LDY #$00
        STY AUDV0
        STY COLUBK

; Clear memory (skip $80 though as it still contains the requested multiload ID)
clear:
        STY $81,x
        DEX
        BPL clear

; Clear bank 0, page 7
        LDX #0
clearp7:
        LDY $F000
        NOP
        LDY $F700,x
        INX
        BNE clearp7

; Copy wait-for-load snipped to RIOT RAM (11 bytes)
        LDX #11
copywaitforload:
        LDA waitforload,X
        STA $F0,X
        DEX
        BPL copywaitforload

; Move the multiload ID to A, then jump to wait-for-load
        LDA $80
        JMP $F0

; The load is done; copy the trampoline to RIOT RAM (6 bytes)
prepareexec:
        LDX #6
copyexec:
        LDA execute,X
        STA $F0,X
        DEX
        BPL copyexec

; The cartridge emulation will provide the load parameters at 0xfff0 -- 0xfff3
; Prepare the control byte
        LDX $FFF0
        STX $80
        LDY $F000,X
; The next value is random value to stick into A
        LDA $FFF1
; The entry point comes next; patch it into the trampoline
        LDX $FFF2
        STX $F4
        LDX $FFF3
        STX $F5

; Setup the registers (we have randomized A above)
        LDX #$FF
        LDY #0
        TXS

; jump into the trampoline and continue execution
        JMP $F0

; ===
; Wait for the cartridge emulation to load the new multiload into RAM.
; This will be executed from RIOT RAM.
; ===
waitforload:
; Write the load ID to $FFF9. This will cause the cartridge emulation to
; copy in the new multiload
        STA $FFF9
wait:
; As long as the cartridge is busy, the data bus will be undriven, and the canary
; load will return $FB
        LDA $FFFB
        BNE wait
; We got 0? The cartridge is driving the bus again, so the load is finished, and
; we can continue
        JMP prepareexec

; ===
; Trampoline
;
; Setup the control register and jump back into the code. This will be
; executed from RIOT RAM.
; ===
execute:
; Trigger the write to the control register...
        STA $FFF8
; ... and jump. The address will be patched in.
        JMP $0000
