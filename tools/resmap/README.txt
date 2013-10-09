The resources.csv file contains a master list of all resources in the
game, as well as both integer and string names for each one.  Running
gen_resfiles.py will cause it to regenerate uqm.rmp, all the .ls2
files, and the i*.h source headers that produce the #defines.

XXX The information below is out of date. See the section 'Updating
the core resources' in sc2/doc/devel/resources for an up-to-date
description of the contents of the resources.csv file.


The .csv file itself is a comma-separated-values list with nine
fields:

1. The header file that will define the constant, with "src/" as its
   base directory
2. The resource index that will define the integer resource, with
   "content/" is its base directory
3. The name of the #defined constant
4. The integral resource package
5. The integral resource instance
6. The integral resource type 
7. The string resource id
8. The string resource type
9. The resource file that contains the content to be loaded

The integral resource type is mainly for backwards compatibility, but
it is the index into the array defined by getres.c (so, MUSICRES is 6,
for instance).

If a resource is defined but has no associated content by default
(i.e., for bonus material), the last two fields will be "--".  They
will not receive an entry in the .rmp file, but it will still be legal
to refer to the #defined constant or the integral resource ID.
