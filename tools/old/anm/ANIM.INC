; ANIM.INC.  RunSkipDump fields used in ANIMA.ASM.  MUST MATCH ANIM.H.

BMBODY_RUNSKIPDUMP  =	1	; RunSkipDump body header.

; --- RunSkipDump Bitmap body format.
OP_DUMP		 = 000h
OP_RUN		 = 000h
OP_SKIP		 = 080h
LONG_OP		 = 080h
LONG_SKIP	  = 000h		; sub-ops for LONG_OP.
LONG_DUMP	  = 080h
LONG_RUN	  = 0C0h
MAX_SHORT_DUMP =    127
MAX_LONG_DUMP  = 03fffh
MAX_SHORT_RUN  =    255
MAX_LONG_RUN   = 03fffh
MAX_SHORT_SKIP =    127
MAX_LONG_SKIP  = 07fffh
MIN_RUN		 = 4   ; When in Dump, minimum worthwhile Run. */
MIN_SKIP	 = 2   ; When in Dump, minimum worthwhile Skip. */
MIN_RUN_SKIP = 4   ; When in Run, minimum worthwhile Skip. */

