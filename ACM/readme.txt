TODO
- put the LGPL license
- change the dialog boxes
- allow decompression the same way as in the command-line

In order to build this codec, you need the Windows98 DDK from Microsoft. It can also work
with the Windows2000/ME DDK.

The driver does not support :
* ACM_STREAMOPENF_QUERY on acmStreamOpen

The .inf installer seem to be mostly writen for Windows NT, but it should work on W9x too.
See lameMP3.inf for more info.