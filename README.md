LXB READ
========

Description
-----------

This tool is used to extract the parameter data from LXB files in ASCII format.
LXB is the format used by Luminex bead arrays and is based on the FCS v3.0
standard, see `docs/flow-cytometry-v3_0-standard.txt`.

Installation and usage
----------------------

Type `make` in the root directory of this project.  This will generate a binary
called `lxbread`.  For usage instructions type `./lxbread --help`.

Note that the Makefile calls the `clang` compiler.  It should be possible to
replace this with `gcc` on systems which do not have `clang` installed.

Important notes
---------------

This tool was written to run as fast as possible and with very specific LXB
files in mind.  It **will not** work with general files based on the FCS v3.0
standard and it must run on a little endian machine (e.g. Intel is ok, PowerPC
is not).  Adding support for more features of FCS v3.0 and other machines
should be simple.

Here are some assumptions made:

*   The LXB file must be smaller than 100 Mb (`$BEGINDATA` and `$ENDDATA` are
    ignored)
*   The data must be integral, in list mode, and in little endian byte order
    (`$DATATYPE = I`, `$MODE = L`, `$BYTEORD = 1,2,3,4`)
*   All parameters must be 32 bit (`$PnB = 32`) 
*   At most 99 parameters are supported
*   Unicode in the text segment is not supported
*   Keys or values containing the separator character are not supported
