;-----------------------------------------------------------------
; dalloc.asm
; ----------
; Memory-allocation routines that operate OUTSIDE of the data segment.
; These work with SEGMENT addresses, NOT actual pointers.
; For allocation in the data segment, just use malloc () and free ().
;-----------------------------------------------------------------

	include asm.inc

	HEADER dalloc

	PSEG dalloc

;-----------------------------------------------------------------
; WORD DAlloc (size);
;	UWORD size;
; Allocate a block of "size" bytes and return its segment.  If
; the allocation fails, return NULL.
; Since this uses DOS allocation, it will allocate just enough paragraphs
; for the number of bytes requested.
;-----------------------------------------------------------------

	public _DAlloc
	STARTPROC _DAlloc

	push	bp
	mov	bp,sp

	mov	ah,048h		; function #

; bx = number of paragraphs = (bytes >> 4) + (bytes & 0x0f) ? 1 : 0;
	mov	bx,ARGB[bp]
	mov	cl,4
	shr	bx,cl
	test	word ptr ARGB[bp],0fh
	je	dalloc_p0
	inc	bx
dalloc_p0:

	int	021h
	jnc	dalloc_p1
	xor	ax,ax		; alloc failed, so return NULL
dalloc_p1:

	pop	bp
	ret

	ENDPROC _DAlloc

;-----------------------------------------------------------------
; void SFree (seg);
; Was "DFree", but PLink said that conflicted with "dfree".
; Free the block at the given segment address.
;
; Okay to call with NULL, so safe to call on already freed segment
; variables.  That DOESN'T mean it is okay to maintain two DIFFERENT
; segment variables, and try to free the same region twice through
; two different variables!  Also, it is up to the caller to NULL
; his sgment variable when done.  Maybe we should switch to a model
; where caller passes ADDRESS of segment variable! [sss]
; Always returns null, so can use to null a variable holding a
; segment address:
;	"segname = SFree(segname);"
;-----------------------------------------------------------------

	public _SFree
	STARTPROC _SFree

	push	bp
	mov	bp,sp
	push	es

; if (!seg), don't do anything
	test	word ptr ARGB[bp],0ffffh
	jz	dfree_done

; free the segment
	mov	ah,049h
	mov	es,ARGB[bp]
	int	021h
ifdef DEBUGGING
	jnc	dfree_1
	mov	ax,9999
	push	ax
	call	_crash
dfree_1:
endif

dfree_done:
	xor	ax,ax		; NULL return value so can assign.
	pop	es
	pop	bp
	ret

	ENDPROC _SFree

;-----------------------------------------------------------------

	ENDPS dalloc
	end
