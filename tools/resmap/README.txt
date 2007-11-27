The resmap tool is a set of utilities for generating subsidiary resource 
index files based on the current system.

----

Currently available runnable files:

mastermap.py:   Walks the content tree, finds all the .lst files, and 
                produces a master resource map mapping IDs to each file.

----

Currently available library files:

explorer.py:    Provides routines for locating and parsing .lst files in 
                the content tree.

stridgen.py:    This does most of the grunt work. It provides the 
                makeid() function, which, given a filename, produces a 
                suitable Resource ID string. This is handled with a huge 
                array of regular expressions at present; as the design 
                of the resource tree evolves, this script may be kept in 
                sync with it.
