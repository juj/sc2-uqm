; my generic graphics routines for C programs


	.8086

	.model	large,c

;*****************************************************************************
;			     Program code starts here
;*****************************************************************************


EQUIP_FLAG	EQU	byte ptr ds:[10h] ; (in Video Display Data Area)

CGAbits		EQU	00100000b	; bits for EQUIP_FLAG
MDAbits		EQU	00110000b


	.code

	public	SetVideoMode
SetVideoMode	proc	far

	push	bp		; preserve caller registers
	mov	bp,sp
	push	ds

	mov	ax,40h
	mov	ds,ax		; DS -> Video Display Data Area

	mov	bl,CGAbits	; BL := bits indicating presence of CGA
	mov	al,6[bp]	; AL := desired video mode number

	mov	ah,al		; test if desired mode is monochrome
	and	ah,7
	cmp	ah,7
	jne	L01		; jump if desired mode not 7 or 0Fh

	mov	bl,MDAbits	; BL := bits indicating presence of MDA

L01:	and	EQUIP_FLAG,11001111b
	or	EQUIP_FLAG,bl	; set bits in EQUIP_FLAG

	xor	ah,ah		; AH := 0 (INT 10h function number)

	push	bp
	int	10h		; call ROM BIOS to set the video mode
	pop	bp

	pop	ds		; restore caller registers and return
	pop	bp

	ret

SetVideoMode endp

	public	GetVideoMode
GetVideoMode	proc	far
	
	push	bp
	mov	ah,0Fh		; Get video mode sub-function
	int	10h		; call ROM BIOS to get video mode number
	pop	bp

	xor	ah,ah		; AX := video mode number

	ret

GetVideoMode	endp


; LoadPalette(numcolors, startcolor, paletteaddr)
	public	LoadPalette
LoadPalette	proc	far

	push	bp
	mov	bp,sp

	push	bx
	push	cx
	push	dx
	push	es
	
	mov	bx,8[bp]
	mov	cx,6[bp]
	mov	es,12[bp]
	mov	dx,10[bp]

	mov	ax,1012h
	int	10H

	pop	es
	pop	dx
	pop	cx
	pop	bx
	pop	bp

	ret

LoadPalette    endp


; initialize the mouse to its default values and display it
	public	InitMouse
InitMouse	proc	far

	push	ax

	mov	ax,0
	int	33h
	mov	ax,1
	int	33h

	pop	ax

	ret

InitMouse	endp

; display the mouse cursor
	public	ShowMouse
ShowMouse	proc	far

	push	ax

	mov	ax,1
	int	33h

	pop	ax

	ret
ShowMouse	endp


; hide the mouse cursor
	public	HideMouse
HideMouse	proc	far
	push	ax

	mov	ax,2
	int	33h
	
	pop	ax

	ret
HideMouse	endp

; read the mouse buttons and cursor position
; on exit:
; BX = button state  bit 0 = left button  bit 1 = right button
; CX = cursor x position
; DX = cursor y position
	public	ReadMouse
ReadMouse	proc	far
	push	bp
	mov	bp,sp
	push	ds
	push	si
	push	bx
	push	cx
	push	dx

	mov	ax,3
	int	33h

	lds	si,6[bp]
	mov	[si],bx

	lds	si,10[bp]
	mov	[si],cx

	lds	si,14[bp]
	mov	[si],dx


	pop	dx
	pop	cx
	pop	bx
	pop	si
	pop	ds

	pop	bp

	ret
ReadMouse	endp

; PlayRunSkipDump(sourcebuf, destbuf);
	public PlayRunSkipDump
PlayRunSkipDump	proc	far

ARGB		= 6
PR_SRC_SEG	= ARGB+2
PR_SRC_ADDR	= ARGB
PR_DST_SEG	= ARGB+6
PR_DST_ADDR	= ARGB+4

	push	bp
	mov	bp,sp
	push	si
	push	di
	push	es

	mov	si,PR_SRC_ADDR[bp]
	mov	di,PR_DST_ADDR[bp]
	mov	es,PR_DST_SEG[bp]

	push	ds			; Save DS:DGROUP.
	mov	ds,PR_SRC_SEG[bp]	; SET DS:dstSeg.  NOT DGROUP.

	sub	ch,ch			; SET CH = 0.
	jmp	short nextOp

skip:
	sub	cl,80h			; Strip off sign bit, leaving skip cnt.
	jz	longOp			; cnt==0 indicates a long op.
; --- shortSkip ---
	add	di,cx			; skip # pixels.  (CH=0)
; --- variation on NEXTOP inline to minimize jmp's ---
nextOp:					; Get and decode next op.
	mov	cl,[si]
	inc	si
	jcxz	run
	or	cl,cl			; Test CL's sign bit.
	jl	skip
dump:
	rep movsb			; copy # pixels.  (CH=0)
; --- variation on NEXTOP inline to minimize jmp's ---
	mov	cl,[si]
	inc	si
	or	cl,cl			; Test CL's sign bit.
	jl	skip
	jg	dump
run:
	mov	cl,[si]			; 8-bit unsigned count.
	inc	si
	lodsb				; pixel value.
	rep stosb			; set # pixels to value.  (CH=0)
; --- variation on NEXTOP inline to minimize jmp's ---
	mov	cl,[si]
	inc	si
	jcxz	run
	or	cl,cl			; Test CL's sign bit.
	jl	skip
	jmp	short dump

longOp:		; NOTE: if load into CX, must clear CH afterwards.
	lodsw				; 16-bit unsigned count.
	or	ax,ax			; set flags.
	jle	notLongSkip
;longSkip:
	add	di,ax			; skip # pixels.
	jmp short nextOp
		; longSkip only used for > 2*127, so can't be very many,
		; so don't bother saving one jmp with inline NEXTOP.

notLongSkip:
	jz	stop			; long count of zero means "stop code".
	mov	cx,ax			; may SET CH non-zero.
	sub	ch,80h			; Clear sign bit.
	cmp	ch,40h
	jge	longRun
; --- For maximum speed on longDump, caller should insure src & dst are
; aligned.  To do so, caller must calculate whether
; src DATA will begin on same (odd or even) alignment as dst data.
; If not, first put a 1-byte Dump, which occupies 2 src bytes, thereby
; shifting relative alignment (src moves 2, dst moves 1).
;longDump
	test	si,1			; Insure src word-aligned.
			; In case caller didn't sync src & dst, we chose
			; to align src because we know it is of benefit --
			; aligning dst on 8-bit video cards might not be of
			; any benefit.
	jz	dumpWordAligned
	movsb				; get to word boundary.
	dec	cx
dumpWordAligned:
	shr	cx,1			; Convert byte count to word count.
	jc	dumpOddByte
	rep movsw			; Word-aligned.
longOpDone:
	sub	ch,ch			; SET CH = 0.
	jmp	short nextOp

dumpOddByte:
	rep movsw			; Word-aligned.
	movsb
	jmp	short longOpDone

longRun:
	sub	ch,40h			; Clear "longRun" bit.
	lodsb
	mov	ah,al			; Replicate byte to word value.
	test	di,1			; Insure dst word-aligned.
	jz	runWordAligned
	stosb
	dec	cx
runWordAligned:
	shr	cx,1			; Convert byte count to word count.
	jc	runOddByte
	rep stosw			; Word-aligned.
	jmp	short longOpDone

runOddByte:
	rep stosw			; Word-aligned.
	stosb
	jmp	short longOpDone
	
stop:
	pop	ds			; Restore DS:DGROUP.

	mov	ax,di			; RETURN final dstAddr.
	pop	es
	pop	di
	pop	si
	pop	bp
	ret

PlayRunSkipDump	endp

; clear the screen to a color
; cls(color);
	public	cls
cls	proc	far

	push	bp
	mov	bp,sp

	push	di
	push	es
	push	cx

	mov	ax,0A000h
	mov	es,ax
	xor	di,di
	
	mov	al,6[bp]
	mov	ah,al
	mov	cx,32000
	rep	stosw

	pop	cx
	pop	es
	pop	di

	pop	bp

	ret

cls	endp

	end
