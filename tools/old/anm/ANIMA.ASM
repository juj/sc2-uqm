;-----------------------------------------------------------------
; anima.asm
; ---------
; Assembly Anim stuff.
;-----------------------------------------------------------------

	include asm.inc
	include anim.inc

	HEADER anima

	extrn _printf:far
;-----------------------------------------------------------------

	DSEG
 IF 0
	extrn	_doDump:byte
	public  _TotBytes
runMsg  db	'run %x',13,0
dumpMsg  db	'dump %x',13,0
skipMsg  db	'skip %x',13,0
clipMsg db	'---dest:%x ',0
finalMsg db	'---dest:%x last-row-bytes:%x--- ',0
_TotBytes dw     0
  ENDIF
	ENDDS

	PSEG anima

;-----------------------------------------------------------------
; void ShortDelay(UWORD delay);
; Does a high-speed delay loop.
;-----------------------------------------------------------------

	public _ShortDelay
	STARTPROC _ShortDelay

SD_DELAY	= ARGB

	push	bp
	mov	bp,sp

	mov	cx,SD_DELAY[bp]
sd_loop:
	loop	sd_loop

	pop	bp
	ret

	ENDPROC _ShortDelay


 IF 1	; RunSkipDump format.
;-----------------------------------------------------------------
; UWORD PlayRunSkipDump(UWORD nBytes, srcSeg, srcAddr, dstSeg, dstAddr);
; Finishes playing the Bitmap body with type==RunSkipDump.
; RETURN final dstAddr (one byte past last pixel written).
; NOTE: since a final skip is omitted, this could be anywhere on the screen.
; If there is no final skip, it will be one past the last pixel on the screen.
; For a screen starting at address (offset) 0, its greatest value is
; the # pixels on the screen.
; NOTE: we're ignoring nBytes.  Bad data could cause this to write past
; end of dst buffer, or even to loop forever.
;-----------------------------------------------------------------

	public _PlayRunSkipDump
	STARTPROC _PlayRunSkipDump

PR_N_BYTES	= ARGB
PR_SRC_SEG	= ARGB+2
PR_SRC_ADDR	= ARGB+4
PR_DST_SEG	= ARGB+6
PR_DST_ADDR	= ARGB+8

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

	ENDPROC _PlayRunSkipDump


;-----------------------------------------------------------------
; UWORD MakeRunSkipDump(UWORD src1Seg, src2Seg, finalPixelAddr,
;		UWORD dstSeg, dstAddr, dstLimit, UWORD *nBytesP,
;		UWORD srcWidth, finalRowAddr, finalRowWidth,
;		BOOL inFinalRow, skipPermitted);
; Finishes making the Bitmap body with type==RunSkipDump.
; RETURN TRUE if fit in deltaX.
; SIDE_EFFECT: set deltaX.nBytes, store that many bytes at deltaX.(seg:addr).
; ASSUMES srcs start at Addr=0.
;-----------------------------------------------------------------

ADDR	= 0	; Field-offsets within a far ptr.  16-bit-offset.
SEGM	= 2	;				   Segment.

; --- registers ---
; AL: pixel.	   Value of current src2 pixel.
; AH: runPix.	   Value of pixels in current Run.
; BL: skipInDump.  # skippable bytes in Dump or Run.
; BH: runInDump.   # runnable bytes in Dump.
; CX: wordCnt.  # pixels processed for current op.
; DX: -- temporary.
; DS:SI: src2.
; ES:DI: src1 or dst.	-- careful, ES:DI used for two purposes.

	public _MakeRunSkipDump
	STARTPROC _MakeRunSkipDump

MR_SRC1_SEG	    = ARGB
MR_SRC2_SEG	    = ARGB+2
MR_FINAL_PIXEL_ADDR = ARGB+4
MR_DST_SEG	    = ARGB+6
MR_DST_ADDR	    = ARGB+8	; dst0 -- save so can calculate size.
MR_DST_LIMIT	    = ARGB+10	; dstLimit => dstAddr musn't pass this.
MR_N_BYTES_PTR	    = ARGB+12
MR_SRC_WIDTH	    = ARGB+14
MR_FINAL_ROW_ADDR   = ARGB+16
MR_FINAL_ROW_WIDTH  = ARGB+18
MR_IN_FINAL_ROW	    = ARGB+20
MR_SKIP_PERMITTED   = ARGB+22
; --- locals
MR_SRC1_FAR	= -4	; ES:DI when addressing src1.
MR_DST_FAR	= -8	; ES:DI when addressing dst.
MR_MUST_STOP	= -9	; BOOL mustStop.  Have reached end of src data.
;unused byte... MR_???		= -10	; BOOL ???
MR_X		= -12	; current x-coordinate.
MR_LARGE_SKIP_LIMIT = -14 ; At or beyond here, don't try to do large skip.
MR_WAS_X	= -16	; x-coordinate where op's data started.
mLOCALS		=  16	; Total local space to save.

	push	bp
	mov	bp,sp
	sub	sp,mLOCALS
	push	si
	push	di
	push	es

 IF 0   ; debugging
	mov	_TotBytes,0
 ENDIF
; --- Simplify "incompressible" checking.  We don't care if we are
; slightly over-conservative.  7 allows 3B for long skip, 4B for any
; other op.  This allows us to not check at all on a short skip
; (a >4B series of short skips would never be done -- long skip done instead),
; and to check run or dump overhead without subtracting the size of the
; overhead.
	sub	word ptr MR_DST_LIMIT[bp],7


; --- Simplify check of limit for "large skip" code.  The limit is determined
; by the need to have CLIP_X see the transition to the final scan row.
	mov	dx,MR_FINAL_ROW_ADDR[bp]
	sub	dx,MR_SRC_WIDTH[bp]	; Paranoia -- a whole row before final.
	sub	dx,4			; Make sure CLIP_X gets a chance.
	mov	MR_LARGE_SKIP_LIMIT[bp],dx


; --- Prepare far ptrs for "les di,...".
	sub	di,di			; SET src1.
	mov	ax,MR_SRC1_SEG[bp]
	mov	MR_SRC1_FAR+ADDR[bp],di
	mov	MR_SRC1_FAR+SEGM[bp],ax
	mov	di,MR_DST_ADDR[bp]	; SET dst.  SET DI:dstAddr.
	mov	ax,MR_DST_SEG[bp]
	mov	MR_DST_FAR+ADDR[bp],di
	mov	MR_DST_FAR+SEGM[bp],ax

; --- ASSUME srcs start at Addr=0.	
	sub	si,si			; SET (DS:SI):src2.
	push	ds			; Save DS:DGROUP.
	mov	ds,MR_SRC2_SEG[bp]	; SET DS:dstSeg.  NOT DGROUP.
; --- DGROUP NOT ACCESSIBLE VIA DS BELOW.  USE SS IF NEED DGROUP. ---

	les	di,MR_DST_FAR[bp]	; SET ES:DI:dst.
	mov	ax,BMBODY_RUNSKIPDUMP
	stosw				; *(UWORD far *)dst++ =
					;   2-byte header containing type.
	mov	MR_DST_FAR+ADDR[bp],di	; Save dstAddr, so DI can be re-used.

	les	di,MR_SRC1_FAR[bp]	; *** SET ES:DI:src1 for loops ***

	sub	dx,dx			; 0.
	mov	MR_X[bp],dx		; "x=0;"
	mov	MR_MUST_STOP[bp],dl	; "mustStop = FALSE;"

notInASequence:
	sub	cx,cx			; CX:wordCnt=0.

; ----------------------- Dump -------------------------------------------
; ASSUME ES:DI: src1.
; "pixel" must be set when jump to beDump with "wordCnt != 0".
beDump:
	mov	dx,MR_X[bp]		;
	sub	dx,cx			; CX:wordCnt.
	jge	Dxw
	add	dx,MR_SRC_WIDTH[bp]	; started on previous line
Dxw:	mov	MR_WAS_X[bp],dx

	sub	bx,bx			; SET BL:skipInDump=0, BH:runInDump=0.
	or	cx,cx			; test CX:wordCnt.
	jz	DnoCnt
	inc	bh			; BH:runInDump.  Any pixel is run==1.
DnoCnt:
inDump:
	mov	dx,MR_X[bp]
	cmp	dx,MR_SRC_WIDTH[bp]
	jne	DnoClip
	call	ClipX			; "CLIP_X(stopDump);"
	jnz	stopDump
DnoClip:

	mov	ah,al			; AH,AL: "runPix = pixel;"
	lodsb				; "pixel = *src2++;"	Get&count byte
	inc	word ptr MR_X[bp]	; "x++;"
	inc	cx			; CX: "wordCnt++;"

	cmp	al,es:[di]		; AL:pixel.  ES:DI:src1.
	je	DSkip			; "if (pixel != *src1)"
	mov	bl,0ffh			;   "-1" & inc below => "skipInDump=0"
DSkip:
	inc	bl			; BL: "skipInDump++;"
	inc	di			; DI: "src1++;"

	cmp	al,ah			; "if (pixel != runPix)"
	je	DRun
	sub	bh,bh			;   "0" & inc below => "runInDump=1"
					;   Any pixel is a run of 1.
DRun:
	inc	bh			; BH: "runInDump++;"

	cmp	bl,MIN_SKIP		; "if (skipInDump == MIN_SKIP"
	jne	DnoSkip
	test	byte ptr MR_SKIP_PERMITTED[bp],1  ; "  &&  skipPermitted)"
	jz	DnoSkip
; --- "PUT_DUMP(src2-wordCnt, wordCnt-MIN_SKIP);"
	push	bx
	push	cx
	mov	bx,si			; SET BX:param1.  SI:src2Addr.
	sub	bx,cx			; CX:wordCnt.
	sub	cx,MIN_SKIP		; SET CX:param2 = CX:wordCnt-MIN_SKIP.
	call	PutDump
	pop	cx
	pop	bx

	mov	cx,MIN_SKIP		; CX:wordCnt.
	jmp	beSkip
DnoSkip:

	cmp	bh,MIN_RUN		; "if (runInDump == MIN_RUN)"
	jne	noRID
; --- "PUT_DUMP(src2-wordCnt, wordCnt-MIN_RUN);"
	push	bx
	push	cx
	mov	bx,si			; SET BX:param1.  SI:src2Addr.
	sub	bx,cx			; CX:wordCnt.
	sub	cx,MIN_RUN		; SET CX:param2 = CX:wordCnt-MIN_RUN.
	call	PutDump
	pop	cx
	pop	bx

	mov	cx,MIN_RUN		; CX:wordCnt.
	jmp	short beRun
noRID:

; --- Byte still in Dump.
	cmp	cx,MAX_LONG_DUMP	; "if (wordCnt == MAX_LONG_DUMP)"
	jne	inDump
stopDump:
; --- "PUT_DUMP(src2-wordCnt, wordCnt);"
	push	bx
	push	cx
	mov	bx,si			; SET BX:param1.  SI:src2Addr.
	sub	bx,cx			; CX:wordCnt.
	;				; SET CX:param2 = CX:wordCnt.
	call	PutDump
	pop	cx
	pop	bx

	test	byte ptr MR_MUST_STOP[bp],1
	jnz	Dstop
	jmp	notInASequence
Dstop:	jmp	beStop


; ----------------------- Run -------------------------------------------
; ASSUME ES:DI: src1.
beRun:
	mov	dx,MR_X[bp]		;
	sub	dx,cx			; CX:wordCnt.
	jge	Rxw
	add	dx,MR_SRC_WIDTH[bp]	; started on previous line
Rxw:	mov	MR_WAS_X[bp],dx

	sub	bx,bx			; SET BL:skipInDump=0.
	mov	ah,al			; "runPix = pixel;"
inRun:
	mov	dx,MR_X[bp]
	cmp	dx,MR_SRC_WIDTH[bp]
	jne	RnoClip
	call	ClipX			; "CLIP_X(stopRun);"
	jnz	stopRun
RnoClip:

	lodsb				; "pixel = *src2++;"	Get&count byte
	inc	word ptr MR_X[bp]	; "x++;"
	inc	cx			; CX: "wordCnt++;"

	cmp	al,es:[di]		; AL:pixel.  ES:DI:src1.
	je	RSkip			; "if (pixel != *src1)"
	mov	bl,0ffh			;   "-1" & inc below => "skipInDump=0"
RSkip:
	inc	bl			; BL: "skipInDump++;"
	inc	di			; DI: "src1++;"

; Note: check for byte runnable comes later.

	cmp	bl,MIN_RUN_SKIP		; "if (skipInDump == MIN_RUN_SKIP"
	jne	RnoSkip
	test	byte ptr MR_SKIP_PERMITTED[bp],1  ; "  &&  skipPermitted)"
	jz	RnoSkip
	sub	cx,MIN_RUN_SKIP		; retract skip bytes from wordCnt.
; --- "PUT_RUN(wordCnt, runPix);"
	call	PutRun			; CX:wordCnt, AH:runPix.  Must Preserve.

	mov	cx,MIN_RUN_SKIP		; CX:wordCnt.
	jmp	short beSkip
RnoSkip:

	cmp	al,ah			; "if (pixel != runPix)"
	je	Rrun
	dec	cx			; Retract pixel from Run.
; --- "PUT_RUN(wordCnt, runPix);"
	call	PutRun			; CX:wordCnt, AH:runPix.  Must Preserve.

	mov	cx,1			; CX:wordCnt.
	jmp	beDump

; --- Byte runnable.
Rrun:
	cmp	cx,MAX_LONG_RUN	; "if (wordCnt == MAX_LONG_RUN)"
	jne	inRun
stopRun:
; --- "PUT_RUN(wordCnt, runPix);"
	call	PutRun			; CX:wordCnt, AH:runPix.  Must Preserve.

	test	byte ptr MR_MUST_STOP[bp],1
	jnz	Dstop			; ("jmp beStop" was out of range.)
	jmp	notInASequence


; ----------------------- Skip -------------------------------------------
beSkip:

; ---------------- Attempt to quickly skip a long ways forward.
	mov	dx,MR_LARGE_SKIP_LIMIT[bp]
	sub	dx,si
	jb	inSkip0			; Too close to end.

	push	cx			; Hold CX:wordCnt.
	mov	cx,dx			; Permit skip up to skipLimit.
	shr	cx,1			; As # words.
	mov	dx,cx			; SET DX: # words to cnt down.
	jcxz	skippedAll
	repe cmpsw

	je	skippedAll		; last words eql; cnt stopped us.
	sub	si,2			; Retract the non-matching word.
	sub	di,2
	inc	cx
skippedAll:
	sub	dx,cx			; # skip words found.
	shl	dx,1			; # skip bytes found.

	pop	cx			; Restore CX:wordCnt.
	add	cx,dx			; wordCnt now reflects new skip bytes.
	add	dx,MR_X[bp]		; x += bytes found, mod width.
SmodX:	sub	dx,MR_SRC_WIDTH[bp]	; force it to underflow.
	jnb	SmodX
	add	dx,MR_SRC_WIDTH[bp]	; compensate for underflow.
SxOk:	mov	MR_X[bp],dx
; ----------------
	jmp	short inSkip


; --- Byte skippable.  Final Skip is NOT output prior to Stop.
skippable:
	inc	di			; DI: "src1++;"

inSkip0:
	mov	dx,MR_X[bp]
	cmp	dx,MR_SRC_WIDTH[bp]
	je	Sclip
; --- fall through to "inSkip".

inSkip:
	lodsb				; "pixel = *src2++;"	Get&count byte
	inc	word ptr MR_X[bp]	; "x++;"
	inc	cx			; CX: "wordCnt++;"

	cmp	al,es:[di]		; AL:pixel.  ES:DI:src1.
	je	skippable		; "if (pixel != *src1)"
; --- Byte not skippable.  Put the Skip.  *** AX:temporary.***
; NOTE: Subtracted enough from dstLimit, that we're not bothering to
; check for incompressible (destination buffer overflow) on short skips.
	inc	di			; DI: "src1++;"
	push	ax			; Hold AX.  "jmp SOutDone" for POP.***
; IF CHANGE # regs pushed, must change SOutDone & Sincompressible.

	mov	MR_SRC1_FAR+ADDR[bp],di	; Update src1Addr from DI.
	les	di,MR_DST_FAR[bp]	; SET ES:DI:dst.

	dec	cx			; "wordCnt--;"	Retract pixel.
 IF 0
	call	DumpPutSkip
 ENDIF
	cmp	cx,MAX_SHORT_SKIP
	ja	SnotShort
; --- One Short Skip.
	mov	al,OP_SKIP		; "*dst++ = OP_SKIP | wordCnt;"
	or	al,cl
	stosb
	jmp	short SOutDone

SnotShort:
	cmp	cx,2*MAX_SHORT_SKIP
	ja	Slong
; --- Two Short Skips.
	mov	al,OP_SKIP	 	; "*dst++ = OP_SKIP | MAX_SHORT_SKIP;"
	or	al,MAX_SHORT_SKIP
	stosb
	sub	cx,MAX_SHORT_SKIP	; "wordCnt-= MAX_SHORT_SKIP;"
	mov	al,OP_SKIP		; "*dst++ = OP_SKIP | wordCnt;"
	or	al,cl
	stosb
	jmp	short SOutDone
Slong:
	mov	dx,cx			; SET DX:wordCnt0 = wordCnt.
Slong0:	mov	cx,dx			; "while (wordCnt0) {..."
	jcxz	SOutDone
	cmp	di,MR_DST_LIMIT[bp]	; Check for incompressible.
	ja	Sincompressible
	cmp	cx,MAX_LONG_SKIP    ; "wordCnt = MIN(wordCnt0,MAX_LONG_SKIP);"
	jbe	Sfits
	mov	cx,MAX_LONG_SKIP
Sfits:	sub	dx,cx			; "wordCnt0 -= wordCnt;"
	mov	al,LONG_OP		; "*dst++ = LONG_OP;"
	stosb
	mov	ax,cx			; "*((UWORD far *)dst++ = wordCnt;"
	stosw
	jmp	short Slong0

SOutDone:
	mov	MR_DST_FAR+ADDR[bp],di	; Update dstAddr.
	les	di,MR_SRC1_FAR[bp]	; SET ES:DI:src1.
	pop	ax			; Restore AX. ***
	mov	cx,1			; CX:wordCnt.
	jmp	beDump

Sclip:	call	ClipX			; "CLIP_X(beStop);"
	jnz	beStop
	jmp	short inSkip		; Skip may be any length up to 64KB-1.

Sincompressible:
	pop	ax			; Match "push ax".
	jmp	incompressible

; ----------------------- Stop -------------------------------------------
beStop:
	mov	MR_SRC1_FAR+ADDR[bp],di	; Update src1Addr from DI.
	les	di,MR_DST_FAR[bp]	; SET ES:DI:dst.
	cmp	di,MR_DST_LIMIT[bp]	; Check for incompressible.
	ja	incompressible
	mov	al,LONG_OP		; "*dst++ = LONG_OP;"
	stosb				; Stop is "LongOp #0".
	sub	ax,ax			; 0.
	stosw				; "*(UWORD far *)dst++ = 0;"

	mov	ax,di			; "*nBytesP = dst - dst0;"
	sub	ax,MR_DST_ADDR[bp]
	mov	bx,MR_N_BYTES_PTR[bp]
	mov	ss:[bx],ax		; ASSUME SS=DGROUP.

	mov	ax,1			; RETURN TRUE.
	jmp	short done

; --- Body could not be compressed into given buffer size.
; Consider it incompressible.  (Actually, client may have given a small
; buffer, and may wish to re-try with a larger buffer.)
incompressible:
	sub	ax,ax			; RETURN FALSE.
done:
	pop	ds			; Restore DS: DGROUP.

	pop	es
	pop	di
	pop	si
	mov	sp,bp
	pop	bp
	ret				; FAR back to client.


; ------ CLIP_X() -----------------------------------------------------------
; No parameters, returns flags according to "test inFinalRow".
; DX: -- temporary.  ASSUME caller allows destruction.
; NOTE: moved per-pixel test into each caller, to save time per pixel.
; Therefore, must already know at line end when call ClipX.
ClipX:	mov	word ptr MR_X[bp],0
	test	byte ptr MR_IN_FINAL_ROW[bp],1
	jz	notDone
	mov	byte ptr MR_MUST_STOP[bp],1
 IF 0
	call	DumpFinal
 ENDIF
	mov	dx,1
	or	dx,dx			; clear zero flag.
	retn				; NEAR.
notDone: cmp	si,MR_FINAL_ROW_ADDR[bp]  ; "if (src2Addr == finalRowAddr)"
	jne	notStartingLastLine
	mov	dx,MR_FINAL_ROW_WIDTH[bp] ; "w = finalRowWidth;"
	mov	MR_SRC_WIDTH[bp],dx
	mov	byte ptr MR_IN_FINAL_ROW[bp],1	  ; "inFinalRow = TRUE;"
notStartingLastLine:
 IF 0
	call	DumpClipX
 ENDIF
	sub	dx,dx			; set zero flag.
	retn				; NEAR.


; ------ Assembly level routines, with same registers & BP as above, except:
; AX: -- temporary.  Each routine must SAVE/RESTORE.
; DX: -- temporary.  ASSUME caller allows destruction.
; REMEMBER, DS:src2Seg, NOT DGROUP.
; REMEMBER, ES:DI: src1.  DI must be updated to MR_SRC1_FAR+ADDR[bp], if
;   need to use it for dst.


; ------ PutRun(CX:wordCnt, AH:runPix) --------------------------------------
; Must Preserve all registers, including parameters.
PutRun:
	jcxz	noRun
 IF 0
	call	DumpPutRun
 ENDIF
	push	ax			; Hold AX.  Last, so can access runPix.
; IF CHANGE # regs pushed, must change PutRun & PutDump & Rincompressible.

	mov	MR_SRC1_FAR+ADDR[bp],di	; Update src1Addr from DI.
	les	di,MR_DST_FAR[bp]	; SET ES:DI:dst.
	cmp	di,MR_DST_LIMIT[bp]	; Check for incompressible.
	ja	Rincompressible
	cmp	cx,MAX_SHORT_RUN	;
	ja	lRun			; "wordCnt > MAX_SHORT_RUN"
	mov	ax,MR_WAS_X[bp]
	add	ax,cx
	cmp	ax,MR_SRC_WIDTH[bp]
	ja	lRun			; "CROSSED_W(wordCnt)"
	mov	al,OP_RUN		; "*dst++ = OP_RUN;"
	stosb
	mov	al,cl			; "*dst++ = wordCnt;"
	stosb
	jmp	short rBody
lRun:					; longRun op.
	mov	al,LONG_OP		; "*dst++ = LONG_OP;"
	stosb
	mov	ax,cx	   		; "*((UWORD far *)dst++ =      "
	or	ah,LONG_RUN		; "   (LONG_RUN<<8) | wordCnt);"
	stosw
rBody:
	pop	ax			; ACCESS AH:runPix.
	push	ax
	mov	al,ah
	stosb				; "*dst++ = runPix;"

	mov	MR_DST_FAR+ADDR[bp],di	; Update dstAddr.
	les	di,MR_SRC1_FAR[bp]	; SET ES:DI:src1.

	pop	ax			; Restore AX.
noRun:	retn				; NEAR.

Dincompressible:
	; NOTE: since we're throwing everything away, doesn't matter that
	; bx & cx aren't the top two items at this point.
	pop	ax			; Balance CALLER'S PUSH BX & CX.
	pop	ax
Rincompressible:
	pop	ax			; Balances "push ax".
	pop	ax			; Remove (near) return address.
	jmp	short incompressible

; --- PutDump(DS:BX:body, CX:cnt).
; BX: param1.	     ASSUME caller allows destruction.
; CX: param2.	     ASSUME caller allows destruction.
; Dump's data is at "body" -- a sequence of pixels in src2seg.
; ALL CALLER'S MUST PUSH BX & CX, AND ONLY those registers, so that
;  "Dincompressible" will pop correct #words.
PutDump:
	jcxz	noDump			; No data to dump.
 IF 0
	call	DumpPutDump
 ENDIF
	push	ax			; Hold AX.
; IF CHANGE # regs pushed, must change PutRun & PutDump & Rincompressible.

	mov	MR_SRC1_FAR+ADDR[bp],di	; Update src1Addr from DI.
	les	di,MR_DST_FAR[bp]	; SET ES:DI:dst.

	mov	dx,MR_DST_LIMIT[bp]	; Check for incompressible.
	sub	dx,cx
	jb	Dincompressible		; (underflow -- "dstLimit < wordCnt")
	cmp	di,dx
	ja	Dincompressible		; "dstAddr > dstLimit-wordCnt"
			; "Rinc..." because "incompressible" was out of range.
			; It must be same condition test ("ja")!

	cmp	cx,MAX_SHORT_DUMP	;
	ja	lDump			; "cnt > MAX_SHORT_DUMP"
	mov	ax,MR_WAS_X[bp]
	add	ax,cx
	cmp	ax,MR_SRC_WIDTH[bp]
	ja	lDump			; "CROSSED_W(cnt)"
	mov	al,OP_DUMP		; "*dst++ = OP_DUMP | cnt;"
	add	al,cl
	stosb
	jmp	short dBody
lDump:					; longDump op.
	mov	al,LONG_OP		; "*dst++ = LONG_OP;"
	stosb
	mov	ax,cx			; "*((UWORD far *)dst++ =   "
	or	ah,LONG_DUMP		; "   (LONG_DUMP<<8) | cnt);"
	stosw
dBody:
	xchg	bx,si			; Hold SI:src2.  SET BX:body.
	rep movsb			; DS:SI:body, ES:DI:dst, CX:cnt.
					; Note: dst is ++ past the new data.
	xchg	bx,si			; Restore SI:src2.

	mov	MR_DST_FAR+ADDR[bp],di	; Update dstAddr.
	les	di,MR_SRC1_FAR[bp]	; SET ES:DI:src1.
	pop	ax			; Restore AX.
noDump:	retn				; NEAR.

 IF 0	; debugging
DumpPutRun:
	push	ds
	push	es
	push	ax
	push	bx
	push	cx
	push	dx
	mov	al,ah
	sub	ah,ah
	push	ax
	push	cx
	mov	ax,ss
	mov	ds,ax
	mov	es,ax
	lea	ax,DGROUP:runMsg
	push	ax
	test	_doDump,0ffh
	jz	noR
	add 	_TotBytes,cx
	call	_printf
noR:
	add	sp,6
	pop	dx
	pop	cx
	pop	bx
	pop	ax
	pop	es
	pop	ds
	retn

DumpPutDump:
	push	ds
	push	es
	push	ax
	push	bx
	push	cx
	push	dx
	push	cx
	mov	ax,ss
	mov	ds,ax
	mov	es,ax
	lea	ax,DGROUP:dumpMsg
	push	ax
	test	_doDump,0ffh
	jz	noD
	add 	_TotBytes,cx
	call	_printf
noD:
	add	sp,4
	pop	dx
	pop	cx
	pop	bx
	pop	ax
	pop	es
	pop	ds
	retn

DumpPutSkip:
	push	ds
	push	es
	push	ax
	push	bx
	push	cx
	push	dx
	push	cx
	mov	ax,ss
	mov	ds,ax
	mov	es,ax
	lea	ax,DGROUP:skipMsg
	push	ax
	test	_doDump,0ffh
	jz	noS
	add 	_TotBytes,cx
	call	_printf
noS:
	add	sp,4
	pop	dx
	pop	cx
	pop	bx
	pop	ax
	pop	es
	pop	ds
	retn

DumpClipX:
	push	ds
	push	es
	push	ax
	push	bx
	push	cx
	push	dx
	mov	ax,ss
	mov	ds,ax
	mov	es,ax
	push	di			; dest
	lea	ax,DGROUP:clipMsg
	push	ax
	test	_doDump,0ffh
	jz	noC
	call	_printf
noC:
	add	sp,4
	pop	dx
	pop	cx
	pop	bx
	pop	ax
	pop	es
	pop	ds
	retn

DumpFinal:
	push	ds
	push	es
	push	ax
	push	bx
	push	cx
	push	dx
	mov	ax,ss
	mov	ds,ax
	mov	es,ax
	mov	ax,MR_FINAL_ROW_WIDTH[bp]
	push	ax
	push	di			; dest
	lea	ax,DGROUP:finalMsg
	push	ax
	test	_doDump,0ffh
	jz	noF
	call	_printf
noF:
	add	sp,6
	pop	dx
	pop	cx
	pop	bx
	pop	ax
	pop	es
	pop	ds
	retn
 ENDIF  ; Debugging
	ENDPROC _MakeRunSkipDump
 ENDIF	; RunSkipDump format.


 IF 0	; simple dump format.
;-----------------------------------------------------------------
; void AsmPlayDelta(UWORD picSeg, deltaSeg, deltaAddr, deltaBytes);
; Finishes playing the current delta-encoding format.
;-----------------------------------------------------------------

	public _AsmPlayDelta
	STARTPROC _AsmPlayDelta

APD_PIC_SEG	= ARGB
APD_DELTA_SEG	= ARGB+2
APD_DELTA_ADDR	= ARGB+4
APD_DELTA_BYTES	= ARGB+6

	push	bp
	mov	bp,sp
	push	si
	push	di
	push	es

	mov	si,APD_DELTA_ADDR[bp]
	mov	dx,APD_DELTA_BYTES[bp]
	mov	es,APD_PIC_SEG[bp]

	push	ds			; Save DS:DGROUP.
	mov	ds,APD_DELTA_SEG[bp]	; SET DS:deltaSeg.  NOT DGROUP.

	jmp	short more		; In case no data.

runLoop:
	lodsw
	mov	di,ax			; SET DI:picAddr
	lodsw
	mov	cx,ax			; SET CX:runCount
	rep movsb			; far_movmem

more:	cmp	si,dx			; while (deltaAddr < deltaBytes)
	jb	runLoop

	pop	ds			; Restore DS:DGROUP.

	pop	es
	pop	di
	pop	si
	pop	bp
	ret

	ENDPROC _AsmPlayDelta


;-----------------------------------------------------------------
; UWORD MakeDeltaSequence(UWORD picSeg, prevSeg, minAddr, maxAddr,
;			 deltaSeg, deltaAddr, limitAddr);
;
; Given a sequence of bytes in the same position in two images,
; create a delta-description to convert one image to the other.
;
; Puts the delta at (deltaSeg,deltaAddr).
; RETURNS "deltaBytes" -- # bytes to describe the delta, INCLUDING
; the initial "deltaAddr" value.
; "limitAddr" is offset into deltaSeg that may not be overwritten.
; RETURNS "-1" if doesn't fit.
;-----------------------------------------------------------------

DOESNT_FIT	= -1

O_PIC_SEG	= ARGB
O_PREV_SEG	= ARGB+2
O_MIN_ADDR	= ARGB+4
O_MAX_ADDR	= ARGB+6
O_DELTA_SEG	= ARGB+8
O_DELTA_ADDR	= ARGB+10
O_LIMIT_ADDR	= ARGB+12

	public	  _MakeDeltaSequence
	STARTPROC _MakeDeltaSequence

	push	bp
	mov	bp,sp
	push	si
	push	di
	push	es

	mov	si,O_MIN_ADDR[bp]	; SET SI:picAddr = minAddr.
	mov	es,O_PREV_SEG[bp]	; SET ES:prevSeg
	mov	dx,O_DELTA_SEG[bp]	; SET DX:deltaSeg
	mov	di,O_DELTA_ADDR[bp]	; SET DI:deltaAddr

	push	ds			; Save DS:DGROUP.
	mov	ds,O_PIC_SEG[bp]	; SET DS:picSeg.  NOT DGROUP.

; --- Not in a run.  Look for differences.
	sub	cx,cx			; SET CX:runCount.
	sub	bx,bx			; SET BX:gapSize.
	jmp	short growGap0

gapStarted:
growGap:
	inc	bx			; BX:gapSize.
	inc	si
growGap0:
	cmp	si,O_MAX_ADDR[bp]
	ja	endWhileInGap

; --- Cmp pic byte to prev byte.
	mov	al,es:[si]		; ES:prevSeg, SI:picAddr
	cmp	al,[si]			; DS:picSeg,  SI:picAddr
	je	growGap

; --- A run is starting.  Determine whether to append to previous run,
;	or to output previous run.
	jcxz	noPreviousRun		; So won't append initial gap to
					; non-existent run.

	cmp	bx,4			; BX:gapSize
	jb	smallGap

endWhileInGap:
	jcxz	done		; NO RUN EVER FOUND.
; --- Output previous run. ---

	mov	ax,di		; Test whether would reach limitAddr.
	add	ax,4		; Run's addr & count.
	add	ax,cx		; runCount.
	cmp	ax,O_LIMIT_ADDR[bp]
	jb	fits
	mov	ax,DOESNT_FIT	; RETURN "DOESNT_FIT".
	jmp	short fail

fits:
	mov	ax,es
	xchg	ax,dx
	mov	es,ax		; SET ES:deltaSeg, DX:prevSeg

	sub	si,bx		; Move back past gap.
	sub	si,cx		; SET SI:runAddr = picAddr - runCount
	mov	ax,si		; *(WORD)(deltaSeg,deltaAddr)= runAddr,
	stosw			; deltaAddr += 2.
	mov	ax,cx		; *(WORD)(deltaSeg,deltaAddr)= runCount,
	stosw	     		; deltaAddr += 2.
	rep movsb	; far_movmem picSeg|Addr => deltaSeg|Addr,
			; runCount bytes.  When done, SI:picAddr set past run,
			; CX:runCount = 0.
	add	si,bx		; Move forward over gap.
	mov	ax,es
	xchg	ax,dx
	mov	es,ax		; SET ES:prevSeg, DX:deltaSeg

	cmp	si,O_MAX_ADDR[bp]	; Extra check saved a few code bytes.
	ja	done

	jmp	short gapConsumed

smallGap:
	add	cx,bx			; Append gap to run.
;	jmp	short gapConsumed
	
noPreviousRun:
gapConsumed:
	sub	bx,bx			; SET BX:gapSize

; --- When reach here, have already found one byte of run.
;     ASSUME SI:picAddr still pointing at byte, CX:runCount already is zero.

continueRun:
	inc	si		    	; Move SI:picAddr past byte of run.
	inc	cx			; CX:runCount.
	cmp	si,O_MAX_ADDR[bp]
	ja	endWhileInRun

	mov	al,es:[si]		; ES:prevSeg, SI:picAddr
	cmp	al,[si]			; DS:picSeg,  SI:picAddr
	jne	continueRun
endWhileInRun:
	jmp	short gapStarted	; SI points to first byte of gap.
					; BX:gapSize = 0.

done:
	mov	ax,di			; RETURN deltaBytes = DI:deltaAddr.
fail:
	pop	ds			; Restore DS:DGROUP.

	pop	es
	pop	di
	pop	si
	pop	bp
	ret

	ENDPROC _MakeDeltaSequence
 ENDIF	; simple dump format.

	ENDPS anima
	end
