;---------------------------------------------------------------------------
; 				ega.mac
; This header file defines subroutines to do ega graphics from assembler.
;-----------------------------------------------------------------

; EGA register equates

TS_INDEX   	equ	3c4h		; Time Sequencer Index 
TS_MASK		equ	2			;   --  Map Mask (Enables CPU writes)
GDC_INDEX	equ	3ceh    	; Graphics Data Controller Index
GDC_MODE	equ	5			;   --  Mode Register
GDC_RSEL	equ	4			;   --  Read Map Select
CRTC_INDEX	equ	3d4h		; CRT Controller Index
CRTC_OFFSET	equ	13h			;   --  Offset Register
CRTC_STARTH	equ	0ch			;   --  High Byte of Start Address
CRTC_STARTL	equ	0dh			;   --	Low Byte of Start Adrress
STATUS_REGISTER	equ	3dah	; EGA status register
VSYNC_MASK	equ	08h			;   --  Vertical synch bit in status register

;-------------------------------------------------------------------------
;	SETREG	macro to set internal register <reg> of chip with
;               index register at <index> to the value stored in AL
;-----------------------------------------------------------------

;	push	dx
;	mov		dx,P1
;	mov		ah,al
;	mov		al,P2
;	out		dx,ax
;	pop		dx

SETREG macro P1, P2
	push	dx
	mov		dx,P1
	mov		ah,al
	mov		al,P2
	out		dx,al
	mov		al,ah
	inc		dx
	out		dx,al
	pop		dx
	endm
