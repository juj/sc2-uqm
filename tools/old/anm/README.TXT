
 Programmer's Kit for                        Version 1.0
 DeluxePaint Animation                       10/24/90
 by Electronic Arts
 
 ----------------------
 This kit has two uses:
 
 1. It contains a program, ANIM2PCX.EXE, which is readily used to
 (1) break an animation file into individual picture files for
     each frame,  or
 (2) to join a group of picture files into an animation file for
     editting in DPaint Animation or for playback with PLAY.EXE.
 
  (PLAY.EXE is included in the DeluxePaint Animation package and may be
   copied and distributed by anyone who has purchased DPaint Animation.)
 
 The picture files are in the PCX format, which can be read by
 DeluxePaint Animation, DeluxePaint II Enhanced and other paint programs,
 and by desktop publishing programs and graphics-importing word processors.
 
 For instructions on using the program as provided, type:
    anim2pcx
 
 
 2. Since the source code for ANIM2PCX is included on this disk,
 a C programmer can access animations from his/her own application,
 or write a converter between the ANM format and some other format,
 by including the subroutines required to read and write ".ANM" files.
 You will need all the enclosed .C, .H, .ASM, .INC files,
 except for PBIO.C and GIO.C (which are used to read/write PCX files),
 and ANIM2PCX.C (which is the main program that you will replace with
 your own code).
 For example, to read the frames of an anim, you might write code
 similar to "AnimFileToPCXFiles()" routine in ANIM2PCX.C
 
 The only existing documentation on the ANM format
 is in comments in LPFILE.C.
 
 
 ---------------------
 The source code for ANIM2PCX may be freely incorporated into
 commercial programs.
 
 Disclaimer:
   THIS PROGRAM IS PROVIDED WITHOUT SEPARATE CHARGE AND IS
   PROVIDED AS IS.  ELECTRONIC ARTS DISCLAIMS ALL WARRANTIES,
   INCLUDING IMPLIED WARRANTIES AND WARRANTIES OF
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.

