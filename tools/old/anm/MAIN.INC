;-----------------------------------------------------------------
; prism.inc
; ---------
; Assembly-language equivalent of prism.h.
; Changed for use with file conversion program.
;	- changed MAX_PLANES from 4 to 8.  Altered BITMAP_OFFS, SIZEOF_BITMAP
;		accordingly.
;-----------------------------------------------------------------

; comment these two out to eliminate debugging code!
; DEBUGGING equ 1
;	extrn _crash:far

; --- miscellaneous constants
POSITIVE		equ 1
NEGATIVE		equ 0

; --- display types.  also in test.h.
EGA_640x200_16		equ 0
EGA_640x350_16		equ 1
CGA_320x200_4		equ 2
CGA_640x200_2		equ 3
EGA_320x200_16		equ 4
HERC_720x348_2		equ 5
MCGA_640x480_2		equ 6
VGA_640x480_16		equ 7
MCGA_320x200_256	equ 8
TANDY_320x200_16	equ 9
VGA_320x200_16		equ 10	; same as EGA mode, but with VGA color control
VGA_640x200_16		equ 11	; same as EGA mode, but with VGA color control
VGA_640x350_16		equ 12	; same as EGA mode, but with VGA color control

MAX_LINE_SIZE		equ 320
MAX_COLORS		equ 256
MAX_PLANES		equ 8
MAX_PLANES_SHIFT	equ 3	; MAX_PLANES expressed as left shift

; --- drawing modes.
MODE_COPY		equ 0
MODE_XOR		equ 1
MODE_COLOR		equ 2
MODE_SWAP		equ 3
MODE_SHADE_UP		equ 4
MODE_SHADE_DOWN 	equ 5
MODE_BMCOPY		equ 6
MODE_INV_COLOR		equ 7
; these two are only used by smear_copy
MODE_SMEAR		equ 8
MODE_BLEND		equ 9
MODE_SHADE_COPY 	equ 10

; --- offsets to parts of the BITMAP structure
; NOTE: drawchar.asm contains hard-coded inits for a BITMAP
BITMAP_FLAGS		equ 0
BITMAP_WIDTH		equ 2
BITMAP_BOX		equ 4
BITMAP_BOX_X		equ 4
BITMAP_BOX_Y		equ 6
BITMAP_BOX_W		equ 8
BITMAP_BOX_H		equ 10
BITMAP_PLANES		equ 12
BITMAP_YTABLE		equ 14
BITMAP_SEG		equ 16
BITMAP_OFFS		equ 32
; --- size of the BITMAP structure
SIZEOF_BITMAP		equ 34

; --- offsets to parts of the Box structure
BOX_X			equ 0
BOX_Y			equ 2
BOX_W			equ 4
BOX_H			equ 6
; --- size of the Box structure
SIZEOF_BOX		equ 8

; --- offsets to parts of the TMPRAS structure
TMPRAS_SEG		equ 0
TMPRAS_SIZE		equ 2

; --- offsets to parts of the RASTPORT structure
RASTPORT_BITMAP 	equ 0
RASTPORT_MODE		equ 2
RASTPORT_COLOR		equ 4

; bitmap.flags values
EGA_BANK_SWITCH 	equ 1
LINEAR_BITMAP		equ 2
NEED_BYTE_ALIGNMENT	equ 4

; properties of screen formats
COLOR_CYCLE		equ 1h		; can color cycle
MONO_MODE		equ 2h		; mode is on mono screen
CGA_MODE		equ 4h		; mode needs CGA adapter
EGA_MODE		equ 8h		; mode needs EGA adapter
MCGA_MODE		equ 10h 	; mode needs MCGA adapter
VGA_MODE		equ 20h 	; mode needs VGA adapter
TANDY_MODE		equ 40h 	; mode needs TGA (Tandy) adapter

; from event.c
MAX_CURSOR_H		equ 11
