;-----------------------------------------------------------------
; misca.asm
; ---------
; Miscellaneous minor assembly-language routines.
;-----------------------------------------------------------------

	include asm.inc
	include main.inc
	include ega.inc

	HEADER misca

;-----------------------------------------------------------------

; --- seg & offs of BIOS equipment flag word (we only use its lower byte)
EQUIP_SEG	equ 40h
EQUIP_OFFS	equ 10h

	DSEG

; --- Most recent i/o error code.  NOTE: if there was not an error,
;     this is undefined.
	public	_ioError
_ioError dw	0

; --- used by shift_right
	public right_shift_masks
right_shift_masks equ this word
	db 11111111b, 11111111b
	db 01111111b, 11111111b
	db 00111111b, 11111111b
	db 00011111b, 11111111b
	db 00001111b, 11111111b
	db 00000111b, 11111111b
	db 00000011b, 11111111b
	db 00000001b, 11111111b
	db 00000000b, 11111111b
	db 00000000b, 01111111b
	db 00000000b, 00111111b
	db 00000000b, 00011111b
	db 00000000b, 00001111b
	db 00000000b, 00000111b
	db 00000000b, 00000011b
	db 00000000b, 00000001b

; --- used by shift_left
left_shift_masks equ this word
	db 11111111b, 11111111b
	db 11111111b, 11111110b
	db 11111111b, 11111100b
	db 11111111b, 11111000b
	db 11111111b, 11110000b
	db 11111111b, 11100000b
	db 11111111b, 11000000b
	db 11111111b, 10000000b
	db 11111111b, 00000000b
	db 11111110b, 00000000b
	db 11111100b, 00000000b
	db 11111000b, 00000000b
	db 11110000b, 00000000b
	db 11100000b, 00000000b
	db 11000000b, 00000000b
	db 10000000b, 00000000b

	ENDDS

;-----------------------------------------------------------------

	PSEG misca

;-----------------------------------------------------------------
; WORD dataseg ();
; Returns the current value of the DS register.
;-----------------------------------------------------------------

	public _dataseg
	STARTPROC _dataseg

	mov	ax,ds
	ret

	ENDPROC _dataseg

;-----------------------------------------------------------------
; WORD codeseg ();
; Returns the current value of the CS register.
;-----------------------------------------------------------------

	public _codeseg
	STARTPROC _codeseg

	mov	ax,cs
	ret

	ENDPROC _codeseg

;-----------------------------------------------------------------
; void far_setmem (seg, offs, length, value);
;	WORD seg, offs;
;	UWORD length;
;	WORD value;
; Set length bytes starting at seg:offs to value.
; NOTE: This could be optimized by using stosw if possible.
;-----------------------------------------------------------------

	public _far_setmem
	STARTPROC _far_setmem

	push	bp
	mov	bp,sp
	push	es
	push	si
	push	di

	mov	es,ARGB[bp]	; segment
	mov	di,ARGB+2[bp]	; offset
	mov	cx,ARGB+4[bp]	; count
	mov	al,ARGB+6[bp]	; value
	jcxz	far_setmem_done
	cld
	rep	stosb
far_setmem_done:
	pop	di
	pop	si
	pop	es
	pop	bp
	ret

	ENDPROC _far_setmem

;-----------------------------------------------------------------
; void far_movmem (sseg, soffs, dseg, doffs, length);
;	WORD sseg, soffs, dseg, doffs;
;	UWORD length;
; Move length bytes starting at sseg:soffs to dseg:doffs.
; This proc doesn't handle overlapping source & dest.
; NOTE: Could speed this up by using WORD move!
;-----------------------------------------------------------------

	public _far_movmem
	STARTPROC _far_movmem

	push	bp
	mov	bp,sp
	push	ds
	push	es
	push	si
	push	di

	mov	ds,ARGB[bp]	; source seg
	mov	si,ARGB+2[bp]	; source offs
	mov	es,ARGB+4[bp]	; dest seg
	mov	di,ARGB+6[bp]	; dest offs
mov0:
	mov	cx,ARGB+8[bp]	; # bytes
	jcxz	far_movmem_done
move_left_to_right:
	cld
	rep	movsb
far_movmem_done:
	pop	di
	pop	si
	pop	es
	pop	ds
	pop	bp
	ret

	ENDPROC _far_movmem

;-----------------------------------------------------------------
; void far_movmem_same_seg(UWORD seg, UWORD soffs, UWORD doffs, UWORD length);
; Move length bytes starting at seg:soffs to seg:doffs,
; dealing with possible OVERLAP --
; when destination starts AFTER source, move RIGHT-TO-LEFT.
; NOTE: Could speed this up by using WORD move!
; NOTE: must have same register push/pop sequence as "_far_movmem".
;-----------------------------------------------------------------

	public _far_movmem_same_seg
	STARTPROC _far_movmem_same_seg

	push	bp
	mov	bp,sp
	push	ds
	push	es
	push	si
	push	di

	mov	ds,ARGB[bp]	; source seg
	mov	si,ARGB+2[bp]	; source offs
	mov	es,ARGB[bp]	; dest seg
	mov	di,ARGB+4[bp]	; dest offs
	mov	cx,ARGB+6[bp]	; # bytes
	jcxz	far_movmem_same_seg_done

	cmp	di,si
	jbe	move_left_to_right

	std			; Backwards through memory.
	add	si,cx
	dec	si		; Address of last byte.
	add	di,cx
	dec	di		; Address of last byte.
	rep	movsb
far_movmem_same_seg_done:
	cld
	pop	di
	pop	si
	pop	es
	pop	ds
	pop	bp
	ret

	ENDPROC _far_movmem_same_seg

;-----------------------------------------------------------------
; void far_swapmem (sseg, soffs, dseg, doffs, length);
;	WORD sseg, soffs, dseg, doffs;
;	UWORD length;
; Swap length bytes starting at sseg:soffs with dseg:doffs.
; This proc doesn't handle overlapping source & dest.
; NOTE: Could speed this up by using WORD operations! (and by using xchg)
;-----------------------------------------------------------------

	public _far_swapmem
	STARTPROC _far_swapmem

	push	bp
	mov	bp,sp
	push	ds
	push	es
	push	si
	push	di

	mov	ds,ARGB[bp]	; source seg
	mov	si,ARGB+2[bp]	; source offs
	mov	es,ARGB+4[bp]	; dest seg
	mov	di,ARGB+6[bp]	; dest offs
	mov	cx,ARGB+8[bp]	; # bytes
	jcxz	far_swapmem_done
	cld
swap_loop:
	mov	al,[si]
	mov	bl,es:[di]
	mov	[si],bl
	mov	es:[di],al
	inc	si
	inc	di
	loop	swap_loop
far_swapmem_done:
	pop	di
	pop	si
	pop	es
	pop	ds
	pop	bp
	ret

	ENDPROC _far_swapmem

;-----------------------------------------------------------------
; LongFrac LFMULT (a, b);
;	LongFrac a, b;
; Multiply two long fractions (ie, 32-bit fixed point numbers with 16 bits
; of binary fraction).
;
; register usage:
;	[bp+ARGB]	a.low
;	[bp+ARGB+2]	a.hi
;	[bp+ARGB+4]	b.lo
;	[bp+ARGB+6]	b.hi
;	ax,dx = result (lo,hi)
;-----------------------------------------------------------------

	public _LFMULT
	STARTPROC _LFMULT

	push	bp
	mov	bp,sp
	push	di
	push	si
	xor	si,si
	mov	ax,[bp+ARGB+2]
	test	ax,ax			; a>0 ?
	jns	aplus
	not	word ptr [bp+ARGB+2]	; a = -a
	neg	word ptr [bp+ARGB]
	sbb	word ptr [bp+ARGB+2],0ffffh
	not	si
aplus:	mov	ax,[bp+ARGB+6]
	test	ax,ax			; b>0?
	jns	bplus
	not	word ptr [bp+ARGB+6]	; b = -b
	neg	word ptr [bp+ARGB+4]
	sbb	word ptr [bp+ARGB+6],0ffffh
	not	si
bplus:	mov	ax,[bp+ARGB]		; get a.lo
	mul	word ptr [bp+ARGB+4]	; a.lo*b.lo
	mov	bx,dx			; res.lo = highhalf(a.lo*b.lo)
	mov	ax,[bp+ARGB+2]
	mul	word ptr [bp+ARGB+6]; a.hi*b.hi
	mov	cx,ax			; res.hi = lowhalf(a.hi*b.hi)
	mov	ax,[bp+ARGB]
	mul	word ptr [bp+ARGB+6]	; a.lo*b.hi
	add	bx,ax
	adc	cx,dx
	mov	ax,[bp+ARGB+2]
	mul	word ptr [bp+ARGB+4]	; a.hi*b.lo
	add	ax,bx			; ax = lo
	adc	dx,cx			; dx = hi
	test	si,si
	jns	leave			; if (neg)
	not	dx			; res = -res
	neg	ax
	sbb	dx,0ffffh
leave:
	pop si
	pop	di
	pop	bp
	ret

	ENDPROC _LFMULT

;-----------------------------------------------------------------
; Return TRUE if there is a char in the keyboard buffer, or FALSE otherwise.
;-----------------------------------------------------------------

	public _char_avail
	STARTPROC _char_avail

	mov	ah,1
	int	16h
	jz	no_char_avail
	mov	ax,-1
	ret
no_char_avail:
	xor	ax,ax
	ret

	ENDPROC _char_avail

;-----------------------------------------------------------------
; Get an "extended" char, where normal chars are in the range 0-255, and
; non-ascii chars (for example, the function keys) are >= 256.
;-----------------------------------------------------------------

	public _get_char
	STARTPROC _get_char

	mov	ah,0
	int	16h
	or	al,al
	jz	gc_done
	xor	ah,ah
gc_done:
	ret

	ENDPROC _get_char

;-----------------------------------------------------------------
; set_ioError: if an io or critical error occurred, squirrels 
; away the error code and clears did_crit_error and returns -1
;
; Assumes that the carry flag and ax are the results of the last
; io operation (int 21h).
;
; If no error occurred, ax is unchanged.
;-----------------------------------------------------------------
did_crit_error	dw 0	;NOTE: didn't include crit error handler in program.

	STARTPROC set_ioError
	pushf				; hold carry flag

	test 	cs:did_crit_error, 0ffffh
	jz	noCritError
	push	cs:did_crit_error      
	pop	_ioError

	popflags
	jmp	short ioerr

noCritError:
	popflags
	jnc	iodone			; okay; leave ax unchanged
	mov	_ioError,ax
ioerr:
	mov	cs:did_crit_error,0
	mov	ax,0ffffh		; return -1 for any error
iodone:	ret
	ENDPROC set_ioError

;-----------------------------------------------------------------
; Open Dos File: fileHandle = opendos (name, access);
;-----------------------------------------------------------------

	public _opendos
	STARTPROC _opendos

	push	bp
	mov	bp,sp
	mov	al,[bp+ARGB+2]	; get access code
	mov	dx,[bp+ARGB]	; pointer to name
	mov	ah,03dh
	int	021h
	call	set_ioError
 	pop	bp
	ret

	ENDPROC _opendos

;-----------------------------------------------------------------
; Create Dos File: fileHandle = creatdos (name);
;-----------------------------------------------------------------

	public _creatdos
	STARTPROC _creatdos

	push	bp
	mov	bp,sp
	xor	cx,cx		; attribute= normal files
	mov	dx,[bp+ARGB]	; pointer to name
	mov	ah,03ch
	int	021h		; do open or create

	jnb	cdx		; if carry flag is set
	mov	ax,0FFFFh	;    return -1
cdx:
	pop	bp
	ret

	ENDPROC _creatdos

;-----------------------------------------------------------------
; Close Dos File: closedos (filehandle);
;-----------------------------------------------------------------

	public _closedos
	STARTPROC _closedos

	push	bp
	mov	bp,sp
	mov	bx,[bp+ARGB]	; get file handle
	mov	ah,03eh
	int	021h		; do read or write
	pop	bp
	ret

	ENDPROC _closedos

;----------------------------------------------------------------------
; Delete Dos File: deletedos (name);  
;---------------------------------------------------------------------

	public _deletedos
	STARTPROC _deletedos

	push	bp
	mov	bp,sp
	mov	dx,ARGB[bp]	; pointer to name
	mov	ah,41h
	int	21h		; delete file
	call	set_ioError
	pop	bp
	ret

	ENDPROC _deletedos

;-----------------------------------------------------------------
; Read from Dos File: nBytes = readdos (file, seg, offset, length);
;-----------------------------------------------------------------

O_FILE		= ARGB
O_SEG		= ARGB+2
O_OFFSET	= ARGB+4
O_LENGTH	= ARGB+6

	public _readdos
	STARTPROC _readdos

	push	bp
	mov	bp,sp
	push	ds
	mov	ah,3fh
	mov	bx,O_FILE[bp]
	mov	cx,O_LENGTH[bp]
	mov	ds,O_SEG[bp]
	mov	dx,O_OFFSET[bp]
	int	021h
	call	set_ioError
	pop	ds
 	pop	bp
	ret

	ENDPROC _readdos

;-----------------------------------------------------------------
; Write to Dos File: nBytes = writedos (file, seg, offset, length);
; RETURN -1 on error.
;-----------------------------------------------------------------

O_FILE		= ARGB
O_SEG		= ARGB+2
O_OFFSET	= ARGB+4
O_LENGTH	= ARGB+6

	public _writedos
	STARTPROC _writedos

	push	bp
	mov	bp,sp
	push	ds
	mov	ah,40h
	mov	bx,O_FILE[bp]
	mov	cx,O_LENGTH[bp]
	mov	ds,O_SEG[bp]
	mov	dx,O_OFFSET[bp]
	int	021h
	call	set_ioError
	pop	ds
 	pop	bp
	ret

	ENDPROC _writedos

;-----------------------------------------------------------------
; LFDIV (num, denom);
;-----------------------------------------------------------------

NUM	=	ARGB
DENOM	=	ARGB+4

	public _LFDIV
	STARTPROC _LFDIV

	push	bp
	mov	bp,sp

; neg = FALSE
	xor	cx,cx

; if (num < 0) { num = -num; neg = !neg; }
	cmp	word ptr NUM+2[bp],0
	jge	lfdiv1
	mov	ax,NUM[bp]		; num = -num
	mov	dx,NUM+2[bp]
	neg	ax
	adc	dx,0
	neg	dx
	mov	NUM[bp],ax
	mov	NUM+2[bp],dx
	not	cx			; neg = !neg
lfdiv1:

; if (denom < 0) { denom = -denom; neg = !neg; }
	cmp	word ptr DENOM+2[bp],0
	jge	lfdiv2
	mov	ax,DENOM[bp]		; denom = -denom
	mov	dx,DENOM+2[bp]
	neg	ax
	adc	dx,0
	neg	dx
	mov	DENOM[bp],ax
	mov	DENOM+2[bp],dx
	not	cx			; neg = !neg
lfdiv2:

; while (denom >= 0x10000) { num >>= 1; denom >>= 1; }
	mov	bx,cx			; save neg in bx
lfdiv5:
	mov	cx,DENOM+2[bp]
	jcxz	lfdiv3
	sar	word ptr NUM+2[bp],1	; num >>= 1
	rcr	word ptr NUM[bp],1
	sar	word ptr DENOM+2[bp],1	; denom >>= 1
	rcr	word ptr DENOM[bp],1
	jmp	lfdiv5
lfdiv3:
	mov	cx,bx			; restore neg to cx

; high word of result = num / denom
	mov	ax,NUM[bp]
	mov	dx,NUM+2[bp]
	cmp	dx,DENOM[bp]		; test for overflow
	jb	no_overflow
	mov	ax,7fffh		; overflow, so use a large #
	xor	dx,dx
	jmp	lfdiv6
no_overflow:
	div	word ptr DENOM[bp]
	mov	bx,ax

; low word of result = ((num % denom) << 16) / denom
lfdiv6:
	xor	ax,ax
	div	word ptr DENOM[bp]
	mov	dx,bx			; dx = high word of result

; if (neg) result = -result;
	jcxz	lfdiv4
	neg	ax
	adc	dx,0
	neg	dx
lfdiv4:

	pop	bp
	ret

	ENDPROC _LFDIV

;-----------------------------------------------------------------
; WORD get_first_mcb ();
; Returns address of first MCB in DOS free memory list.
;-----------------------------------------------------------------

	public _get_first_mcb
	STARTPROC _get_first_mcb

	mov	ah,52h
	int	21h
	mov	ax,es:[bx-2]
	ret

	ENDPROC _get_first_mcb

;-----------------------------------------------------------------
; WORD cdist (a, b);
;	LONG a, b;
; Calculate the distance between RGB color values a and b.  This is
; done using the sum of the absolute R,G,B deltas.  It would be more
; accurate (but much slower) to calculate the actual distance between them.
;
; Improved: absdR + absdG + absdB + MAX(absdR, absdG, absdB).
; This approximates the sum of squares, by forcing large component to
; have double contribution to error term.
;-----------------------------------------------------------------

A	= ARGB
B	= ARGB+4

	public _cdist
	STARTPROC _cdist

	push	bp
	mov	bp,sp

; --- clear top half of ax so we can use it as a word
	sub	ah,ah

; --- get absolute distance between BLUE coords, put result in bx
	mov	bl,A[bp]
	mov	cl,B[bp]
	cmp	bl,cl
	jae	cdist_p1
	xchg	bl,cl
cdist_p1:
	sub	bl,cl
	sub	bh,bh		; BX: abs(delta R).

	mov	dx,bx		; dx will be MAX.

; --- get absolute distance between GREEN coords, add to bx
	mov	al,A+1[bp]
	mov	cl,B+1[bp]
	cmp	al,cl
	jae	cdist_p2
	xchg	al,cl
cdist_p2:
	sub	al,cl
	add	bx,ax		; BX: absdR + absdG.

	cmp	dx,ax
	jge	cdist_R_ge_G
	mov	dx,ax
cdist_R_ge_G:			; DX: MAX(absdR, absdG).

; --- get absolute distance between RED coords, add bx to this value
	mov	al,A+2[bp]
	mov	cl,B+2[bp]
	cmp	al,cl
	jae	cdist_p3
	xchg	al,cl
cdist_p3:
	sub	al,cl
	add	ax,bx		; AX: absdR + absdG + absdB.

	cmp	dx,ax
	jge	cdist_RG_ge_B
	mov	dx,ax
cdist_RG_ge_B:			; DX: MAX(absdR, absdG, absdB).

	add	ax,dx		; AX: absdR + absdG + absdB + MAX(...).

	pop	bp
	ret

	ENDPROC _cdist

;-----------------------------------------------------------------
; void dos_print_string (string);
;	char *string;
; Call DOS to print the given string, in text mode.  The string should
; not contain a '$', since DOS will treat that as the end of the string,
; and not print any chars after that!
;-----------------------------------------------------------------

	public _dos_print_string
	STARTPROC _dos_print_string

	push	bp
	mov	bp,sp
	push	si

; --- replace the NULL terminator with a '$', like DOS wants
	mov	si,ARGB[bp]
	dec	si
find_dollar:
	inc	si
	mov	al,[si]
	or	al,al
	jnz	find_dollar
	mov	byte ptr [si],'$'

; --- call DOS to print the string
	push	si
	mov	ah,9
	mov	dx,ARGB[bp]
	int	21h
	pop	si

; --- restore NULL string terminator
	mov	byte ptr [si],0

	pop	si
	pop	bp
	ret

	ENDPROC _dos_print_string

;-----------------------------------------------------------------
; Allocate Memory:  segment = allocsome (paragraphs, &size)
; Will allocate request or largest remaining, whichever is bigger.
; Returns segment = 0 if failed.  Puts number of paragraphs actually
; allocated into *size.
; Right now, this is only used by FixTandyMemory.
;-----------------------------------------------------------------

	public _allocsome
	STARTPROC _allocsome

	push	bp
	mov	bp,sp
	mov	bx,[bp+ARGB]
allocscont:
	mov	di,[bp+ARGB+2]
	mov	[di],bx 	; return number of words allocated
	mov	ax,0
	cmp	ax,bx
	je	allocsret	; fail nothing requested
	mov	ax,04800h
	int	021h		; execute DOS function
	jnc	allocsret
	mov	cx,ax
	mov	ax,0		; error: return NIL
	cmp	cx,7		; arena trashed
	jne	allocscont	; keep trying until we alloc max left or fail
allocsret:
	pop	bp
	ret

	ENDPROC _allocsome

;-----------------------------------------------------------------
; LONG FreeDiskSpace (drive);
;	WORD drive; /* 0 = drive A, 1 = drive B, etc... */
; Returns free disk space in bytes on the specified drive.  If there
; is an error accessing that drive, it returns 0.
;-----------------------------------------------------------------

	public _FreeDiskSpace
	STARTPROC _FreeDiskSpace

	push	bp
	mov	bp,sp

	mov	ah,36h
	mov	dl,ARGB[bp]
	inc	dl		; this func uses 1 = drive A, 2 = drive B...
	int	21h
	cmp	ax,0ffffh
	jne	fds_no_error
; --- error getting info, so return 0
	xor	ax,ax
	xor	dx,dx
	jmp	fds_done
; --- no error, so return (sectorsPerCluster * bytesPerSector * freeClusters)
; NOTE: we assume that sectorsPerCluster * bytesPerSector < 64k.
fds_no_error:
	mul	cx
	mul	bx
fds_done:
	pop	bp
	ret

	ENDPROC _FreeDiskSpace

;-----------------------------------------------------------------
; LONG GetSystemTime ();
; Returns the number of timer ticks (18.2 per second) since midnight.
;-----------------------------------------------------------------

	public _GetSystemTime
	STARTPROC _GetSystemTime

	mov	ah,0
	int	1ah
	mov	ax,dx		; move low word to ax
	mov	dx,cx		; move high word to dx
	ret

	ENDPROC _GetSystemTime

;-----------------------------------------------------------------
; name		WordMulDiv --
;
; c = (a*num)/denom;
;
; UWORD muldiv(a,num,denom)  UWORD a,num,denom;
;
;  register usage
;		[bp+ARGB]	a
;		[bp+ARGB+2]	num
;		[bp+ARGB+4]	denom
;-----------------------------------------------------------------

	public _WordMulDiv
	STARTPROC _WordMulDiv
	push	bp
	mov	bp,sp
	mov ax,[bp+ARGB]
	mul	word ptr[bp+ARGB+2]
	div word ptr[bp+ARGB+4]
	pop	bp
	ret
	ENDPROC _WordMulDiv

;-----------------------------------------------------------------

	ENDPS misca
	END
