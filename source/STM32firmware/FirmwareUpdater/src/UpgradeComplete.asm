	processor 6502
	include vcs.h
	include macro.h

	seg.u variables
	org $0080

	seg code
	org $ff00

Reset:
 cld		;Must be located at offset 0 in order to activate ROM from SD load screen in unocart
 lda #0
 ldx #$ff
ClearLoop:
 sta 0,x
 dex
 bne ClearLoop

MainLoop:
 lda #2
 sta VSYNC
 sta WSYNC
 sta WSYNC
 sta WSYNC
 lda #0
 sta VSYNC

 ldx #37
VBlank:
 sta WSYNC
 dex
 bne VBlank

 lda #0
 sta WSYNC
 sta VBLANK
 sta PF0
 sta PF1
 sta PF2
 ldx #$0e
 stx COLUBK


 ldx #35 
TopMargin:
 sta WSYNC
 dex
 bne TopMargin

 ldy #0
RenderLoop:
 ldx #11 
DrawRow:
 sta WSYNC
 lda PF0a,y
 sta PF0
 lda PF1a,y
 sta PF1
 lda PF2a,y
 sta PF2
 lda $80
 adc #1
 sta $80
 sta COLUPF
 lda PF0b,y
 sta PF0
 lda PF1b,y
 sta PF1
 lda PF2b,y
 sta PF2
 dex
 bne DrawRow
 iny
 cpy #11
 bne RenderLoop

 lda #0
 sta WSYNC
 sta PF0
 sta PF1
 sta PF2
 ldx #34 
BottomMargin:
 sta WSYNC
 dex
 bne BottomMargin


 lda #2
 sta VBLANK
 ldx #30
Overscan:
 sta WSYNC
 dex
 bne Overscan
 jmp MainLoop

PF0a:
 hex 00 00 00 00 00 00 88 44 44 44 88
PF1a:
 hex 12 12 12 12 0c 00 91 2a 2a 2a 92
PF2a:
 hex 73 95 93 91 71 00 c8 55 d2 50 50 
PF0b:
 hex cc 22 ee 22 22 00 44 55 44 44 cc 
PF1b:
 hex 3b 92 93 92 93 00 3b 21 39 21 b9
PF2b:
 hex 01 00 01 00 01 00 1d 04 1c 04 1c
;    ||   X  X ||XX  XXX ||  XX||  XXX XX||X             
;    ||   X  X ||X X X  X|| X  ||X  X  X ||	             
;    ||   X  X ||XX  X  X|| XXX||X  X  XX||X            
;    ||   X  X ||X   X  X|| X  ||X  X  X ||	           
;    ||    XX  ||X   XXX || X  ||X  X  XX||X         
;    ||        ||        ||    ||        ||              
;   X||X  X   X||   X  XX||  X ||  XXX XX||X XXX  
;  X ||  X X X ||X X X X ||X X ||  X    X||  X 
;  X ||  X X X || X  X XX||  X ||  XXX  X||  XXX
;  X ||  X X X ||    X X ||  X ||  X    X||  X 
;   X||X  X  X ||    X X ||  XX||X XXX  X||  XXX 

	org $ffef
	hex ff

	org $fffc
;reset vector
	.word Reset
	.word Reset


