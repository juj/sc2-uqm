;-----------------------------------------------------------------
; asm.inc
; -------
; Standard macros for DPaint II MS-DOS ".asm" files.
;-----------------------------------------------------------------

;-----------------------------------------------------------------
; Memory model configuration.
;-----------------------------------------------------------------

LPROG	equ	1		; 1 means large (>64k) program segment
LDATA	equ	0		; 1 means large (>64k) data segment

;-----------------------------------------------------------------
; After a standard procedure entry (push bp, mov bp,sp), the first
; parm on the stack can be accessed at ARGB[bp].
;-----------------------------------------------------------------

	if LPROG
ARGB equ 6
	else
ARGB equ 4
	endif

;-----------------------------------------------------------------
; File header: sets up groups, segment names, etc.
;-----------------------------------------------------------------

	if LPROG
HEADER macro module
module&_TEXT segment word public 'CODE'
module&_TEXT ends
_DATA segment word public 'DATA'
_DATA ends
CONST segment word public 'CONST'
CONST ends
_BSS segment word public 'BSS'
_BSS ends
DGROUP group CONST, _BSS, _DATA
	assume cs:module&_TEXT, ds:DGROUP, ss:DGROUP, es:DGROUP
	endm
	else
HEADER macro 
_TEXT segment word public 'CODE'
_TEXT ends
CONST segment word public 'CONST'
CONST ends
_BSS segment word public 'BSS'
_BSS ends
_DATA segment word public 'DATA'
_DATA ends
DGROUP group CONST, _BSS, _DATA
	assume cs:_TEXT, ds:DGROUP, ss:DGROUP, es:DGROUP
	endm
	endif

;-----------------------------------------------------------------
; Data segment start & end.
;-----------------------------------------------------------------

DSEG macro
_DATA segment
	endm

;-----------------------------------------------------------------

ENDDS macro
_DATA ends
	endm

;-----------------------------------------------------------------
; Program segment start & end.
;-----------------------------------------------------------------

	if LPROG
PSEG macro module
module&_TEXT segment
	endm
	else
PSEG macro
_TEXT	segment
	endm
	endif

;-----------------------------------------------------------------

	if LPROG
ENDPS macro module
module&_TEXT ends
	endm
	else
ENDPS macro module
_TEXT ends
	endm
	endif

;-----------------------------------------------------------------
; Procedure start & end.
;-----------------------------------------------------------------

STARTPROC macro proc_name
	if LPROG
proc_name proc far
	else
proc_name proc near
	endif
	endm

;--- start a procedure and declare it public ---
STARTPROC_PUBLIC macro name
	public name
	if LPROG
name proc far
	else
name proc near
	endif
	endm

;-----------------------------------------------------------------

ENDPROC macro proc_name
proc_name endp
	endm

;-----------------------------------------------------------------
; When the 80286 is in real mode, the POPF instruction may recognize
; a pending maskable interrupt even if interrupts were off before the
; corresponding PUSHF.  This macro should be used instead of POPF.
; See "IBM Personal Computer Seminar Proceedings" v2, #4 (September '84)
; for info about the POPF bug.
;-----------------------------------------------------------------

popflags macro
		jmp		$+3		; jump around IRET
		iret			; pop CS, IP, and flags
		push	cs
		call	$-2		; call within segment
		endm

;-----------------------------------------------------------------
; Offsets to low byte & high byte of a word.
;-----------------------------------------------------------------
LO	= 0
HI	= 1

;-----------------------------------------------------------------
; --- miscellaneous constants

TRUE				equ 1
FALSE				equ 0

