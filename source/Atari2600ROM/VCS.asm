; VCS.asm
; MADS compatible version by JAC! 2010-12-15.
; Based on VCS.H version 1.05, 2003-11-13.
; Last update on 2010-11-11.
; The latest version of this file is availble on
; http://www.wudsn.com/productions/atari2600/ide/VCS.asm

VERSION_VCS	= 105

; THIS IS A PRELIMINARY RELEASE OF *THE* "STANDARD" VCS.H
; THIS FILE IS EXPLICITLY SUPPORTED AS A DASM-PREFERRED COMPANION FILE
; PLEASE DO *NOT* REDISTRIBUTE THIS FILE!
;
; This file defines hardware registers and memory mapping for the
; Atari 2600. It is distributed as a companion machine-specific support package
; for the DASM compiler. Updates to this file, DASM, and associated tools are
; available at at http://www.atari2600.org/dasm
;
; Many thanks to the original author(s) of this file, and to everyone who has
; contributed to understanding the Atari 2600.  If you take issue with the
; contents, or naming of registers, please write to me (atari2600@taswegian.com)
; with your views.  Please contribute, if you think you can improve this
; file!
;
; Latest Revisions...
; 1.05  13/NOV/2003	- Correction to 1.04 - now functions as requested by MR.
;			- Added VERSION_VCS equate (which will reflect 100x version #)
;			 This will allow conditional code to verify VCS.H being
;			 used for code assembly.
; 1.04  12/NOV/2003     Added TIA_BASE_WRITE_ADDRESS and TIA_BASE_READ_ADDRESS for
;			convenient disassembly/reassembly compatibility for hardware
;			mirrored reading/writing differences.  This is more a 
;			readability issue, and binary compatibility with disassembled
;			and reassembled sources.  Per Manuel Rotschkar's suggestion.
; 1.03  12/MAY/2003     Added SEG segment at end of file to fix old-code compatibility
;			which was broken by the use of segments in this file, as
;			reported by Manuel Polik on [stella] 11/MAY/2003
; 1.02  22/MAR/2003     Added TIMINT($285)
; 1.01				Constant offset added to allow use for 3F-style bankswitching
;						- define TIA_BASE_ADDRESS as $40 for Tigervision carts, otherwise
;						  it is safe to leave it undefined, and the base address will
;						  be set to 0.  Thanks to Eckhard Stolberg for the suggestion.
;			 Note, may use -DLABEL=EXPRESSION to define TIA_BASE_ADDRESS
;			- register definitions are now generated through assignment
;			 in uninitialised segments.  This allows a changeable base
;			 address architecture.
; 1.0	22/MAR/2003		Initial release


;-------------------------------------------------------------------------------

; TIA_BASE_ADDRESS
; The TIA_BASE_ADDRESS defines the base address of access to TIA registers.
; Normally 0, the base address should (externally, before including this file)
; be set to $40 when creating 3F-bankswitched (and other?) cartridges.
; The reason is that this bankswitching scheme treats any access to locations
; < $40 as a bankswitch.

.if .not .def TIA_BASE_ADDRESS
	TIA_BASE_ADDRESS	= 0
.endif

; Note: The address may be defined on the command-line using the -D switch, eg:
; dasm.exe code.asm -DTIA_BASE_ADDRESS=$40 -f3 -v5 -ocode.bin
; *OR* by declaring the label before including this file, eg:
; TIA_BASE_ADDRESS = $40
;   include "vcs.h"

; Alternate read/write address capability - allows for some disassembly compatibility
; usage ; to allow reassembly to binary perfect copies).  This is essentially catering
; for the mirrored ROM hardware registers.

; Usage: As per above, define the TIA_BASE_READ_ADDRESS and/or TIA_BASE_WRITE_ADDRESS
; using the -D command-line switch, as required.  If the addresses are not defined, 
; they defaut to the TIA_BASE_ADDRESS.

.if .not .def TIA_BASE_READ_ADDRESS
	TIA_BASE_READ_ADDRESS = TIA_BASE_ADDRESS
.endif

.if .not .def TIA_BASE_WRITE_ADDRESS
	TIA_BASE_WRITE_ADDRESS = TIA_BASE_ADDRESS
.endif

;-------------------------------------------------------------------------------

	org TIA_BASE_WRITE_ADDRESS

	; DO NOT CHANGE THE RELATIVE ORDERING OF REGISTERS!

vsync	.ds 1	; $00   0000 00x0   Vertical Sync Set-Clear
vblank	.ds 1	; $01   xx00 00x0   Vertical Blank Set-Clear
wsync	.ds 1	; $02   ---- ----   Wait for Horizontal Blank
rsync	.ds 1	; $03   ---- ----   Reset Horizontal Sync Counter
nusiz0	.ds 1	; $04   00xx 0xxx   Number-Size player/missle 0
nusiz1	.ds 1	; $05   00xx 0xxx   Number-Size player/missle 1
colup0	.ds 1	; $06   xxxx xxx0   Color-Luminance Player 0
colup1	.ds 1	; $07   xxxx xxx0   Color-Luminance Player 1
colupf	.ds 1	; $08   xxxx xxx0   Color-Luminance Playfield
colubk	.ds 1	; $09   xxxx xxx0   Color-Luminance Background
ctrlpf	.ds 1	; $0A   00xx 0xxx   Control Playfield, Ball, Collisions
		;	D0 = REF (reflect playfield)
		;	D1 = SCORE (left half of playfield gets color of player 0, right half gets color of player 1
		;	D2 = PFP (playfield gets priority over players so they can move behind the playfield)
		;	D4 & D5 = BALL SIZE
refp0	.ds 1	; $0B   0000 x000   Reflection Player 0
refp1	.ds 1	; $0C   0000 x000   Reflection Player 1
pf0	.ds 1	; $0D   xxxx 0000   Playfield Register Byte 0
pf1	.ds 1	; $0E   xxxx xxxx   Playfield Register Byte 1
pf2	.ds 1	; $0F   xxxx xxxx   Playfield Register Byte 2
resp0	.ds 1	; $10   ---- ----   Reset Player 0
resp1	.ds 1	; $11   ---- ----   Reset Player 1
resm0	.ds 1	; $12   ---- ----   Reset Missle 0
resm1	.ds 1	; $13   ---- ----   Reset Missle 1
resbl	.ds 1	; $14   ---- ----   Reset Ball
audc0	.ds 1	; $15   0000 xxxx   Audio Control 0
audc1	.ds 1	; $16   0000 xxxx   Audio Control 1
audf0	.ds 1	; $17   000x xxxx   Audio Frequency 0
audf1	.ds 1	; $18   000x xxxx   Audio Frequency 1
audv0	.ds 1	; $19   0000 xxxx   Audio Volume 0
audv1	.ds 1	; $1A   0000 xxxx   Audio Volume 1
grp0	.ds 1	; $1B   xxxx xxxx   Graphics Register Player 0
grp1	.ds 1	; $1C   xxxx xxxx   Graphics Register Player 1
enam0	.ds 1	; $1D   0000 00x0   Graphics Enable Missle 0
enam1	.ds 1	; $1E   0000 00x0   Graphics Enable Missle 1
enabl	.ds 1	; $1F   0000 00x0   Graphics Enable Ball
hmp0	.ds 1	; $20   xxxx 0000   Horizontal Motion Player 0
hmp1	.ds 1	; $21   xxxx 0000   Horizontal Motion Player 1
hmm0	.ds 1	; $22   xxxx 0000   Horizontal Motion Missle 0
hmm1	.ds 1	; $23   xxxx 0000   Horizontal Motion Missle 1
hmbl	.ds 1	; $24   xxxx 0000   Horizontal Motion Ball
vdelp0	.ds 1	; $25   0000 000x   Vertical Delay Player 0
vdelp1	.ds 1	; $26   0000 000x   Vertical Delay Player 1
vdelbl	.ds 1	; $27   0000 000x   Vertical Delay Ball
resmp0	.ds 1	; $28   0000 00x0   Reset Missle 0 to Player 0
resmp1	.ds 1	; $29   0000 00x0   Reset Missle 1 to Player 1
hmove	.ds 1	; $2A   ---- ----   Apply Horizontal Motion
hmclr	.ds 1	; $2B   ---- ----   Clear Horizontal Move Registers
cxclr	.ds 1	; $2C   ---- ----   Clear Collision Latches

dummy3	= cxclr
dummy4	= cxclr+$100

;-------------------------------------------------------------------------------

	org TIA_BASE_READ_ADDRESS

		;					bit 7   bit 6
cxm0p	.ds 1	; $00	xx00 0000	Read Collision  M0-P1   M0-P0
cxm1p	.ds 1	; $01	xx00 0000			M1-P0   M1-P1
cxp0fb	.ds 1	; $02	xx00 0000			P0-PF   P0-BL
cxp1fb	.ds 1	; $03	xx00 0000			P1-PF   P1-BL
cxm0fb	.ds 1	; $04	xx00 0000			M0-PF   M0-BL
cxm1fb	.ds 1	; $05	xx00 0000			M1-PF   M1-BL
cxblpf	.ds 1	; $06	x000 0000			BL-PF   -----
cxppmm	.ds 1	; $07	xx00 0000			P0-P1   M0-M1
inpt0	.ds 1	; $08	x000 0000	Read Pot Port 0
inpt1	.ds 1	; $09	x000 0000	Read Pot Port 1
inpt2	.ds 1	; $0A	x000 0000	Read Pot Port 2
inpt3	.ds 1	; $0B	x000 0000	Read Pot Port 3
inpt4	.ds 1	; $0C	x000 0000	Read Input (Trigger) 0
inpt5	.ds 1	; $0D	x000 0000	Read Input (Trigger) 1

;-------------------------------------------------------------------------------

	org $280	;RIOT
 
 	; RIOT MEMORY MAP

swcha 	.ds 1	; $280	Port A data register for joysticks:
		;	Bits 4-7 for player 1.  Bits 0-3 for player 2.

swacnt	.ds 1	; $281	Port A data direction register (DDR)
swchb	.ds 1	; $282	Port B data (console switches)
swbcnt	.ds 1	; $283	Port B DDR
intim	.ds 1	; $284	Timer output

timint	.ds 1	; $285

	; Unused/undefined registers ($285-$294)

	.ds 1	; $286
	.ds 1	; $287
	.ds 1	; $288
	.ds 1	; $289
	.ds 1	; $28A
	.ds 1	; $28B
	.ds 1	; $28C
	.ds 1	; $28D
	.ds 1	; $28E
	.ds 1	; $28F
	.ds 1	; $290
	.ds 1	; $291
	.ds 1	; $292
	.ds 1	; $293

tim1t	.ds 1	; $294	Set 1 clock interval
tim8t	.ds 1	; $295	Set 8 clock interval
tim64t	.ds 1	; $296	Set 64 clock interval
t1024t	.ds 1	; $297	Set 1024 clock interval


	
	.macro assert_same_page
	.if :1 / $100 <> (:1 + .len :1 - 1) / $100
	.error ":1 crosses page boundary between ", :1, , " - ", :1 + .len :1 - 1
;	.print "ERROR: :1 crosses page boundary between ", :1, , " - ", :1 + .len :1 - 1

	.else
	.print ":1 within page boundary between ", :1, , " - ", :1 + .len :1 - 1
	.endif
	.endm