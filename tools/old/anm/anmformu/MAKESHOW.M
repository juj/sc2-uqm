showanim.obj:		showanim.c showanim.h
cl /c /Zi /AL showanim.c

cgraph.obj:		cgraph.asm
	masm $*;

showanim.exe:		showanim.obj cgraph.obj
	link /co showanim+cgraph;
