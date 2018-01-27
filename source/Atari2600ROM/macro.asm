;-------------------------------------------------------
; Usage: TIMER_SETUP lines
; where lines is the number of scanlines to skip (> 2).
; The timer will be set so that it expires before this number
; of scanlines. A WSYNC will be done first.

.MACRO TIMER_SETUP lines
        lda #(((:lines-1)*76-14)/64)
        sta wsync
        sta tim64t
.ENDM

;-------------------------------------------------------
; Use with TIMER_SETUP to wait for timer to complete.
; You may want to do a WSYNC afterwards, since the timer
; is not accurate to the beginning/end of a scanline.

.MACRO TIMER_WAIT
waittimer
        lda intim
        bne waittimer
.ENDM

.MACRO Sleep cycles
		#cycle #:cycles
.ENDM