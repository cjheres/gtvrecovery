gtvrecovery
===========

GTVHacker Google TV Custom Recovery
Visit www.GTVHacker.com for more information.

To compile:
	- Obtain directfb source from here:
		http://www.sony.net/Products/Linux/TV/Download/common/fKCxbimEfMm4rr7T3qFLag/directfb.tgz

Modify the make script (paths mostly, symlink a few gcc bits)

Rename recovery.c to "df_fire.c", and replace the example with it

Run "make examples"

You will have a "df_fire" binary that should be ready to run.

Of course, this can be streamlined, and the recovery code is garbage.

Please, do improve it, and release your changes. It'd be awesome!

-CJ
