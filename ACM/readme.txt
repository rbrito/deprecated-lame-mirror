In order to build this codec, you need the Windows98 DDK from Microsoft. It can also work
with the Windows2000/ME DDK :

http://www.microsoft.com/ddk/
http://www.microsoft.com/ddk/ddk98.asp
http://www.microsoft.com/ddk/W2kDDK.asp

Alternatively, the required headers are also available in CYGWIN+w32api, MINGW32 or Wine :

http://www.cygwin.com/
http://www.mingw.org/
http://www.winehq.com/

---------------

Define ENABLE_DECODING if you want to use the decoding (alpha state, doesn't decode at the
 moment, so use it only if you plan to develop)
