# Microsoft Developer Studio Project File - Name="MP3EncDll" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 5.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=MP3EncDll - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "MP3EncDll.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "MP3EncDll.mak" CFG="MP3EncDll - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "MP3EncDll - Win32 Release" (based on\
 "Win32 (x86) Dynamic-Link Library")
!MESSAGE "MP3EncDll - Win32 Debug" (based on\
 "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=xicl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "MP3EncDll - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /Zp2 /MT /W3 /GX /Ox /Ot /Og /Ob2 /I "../libmp3lame" /I "../include" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_BLADEDLL" /D "TAKEHIRO_IEEE754_HACK" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o NUL /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o NUL /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /map /machine:I386 /out:"Release\lame_enc.dll"

!ELSEIF  "$(CFG)" == "MP3EncDll - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /Zp2 /MTd /W3 /Gm /GX /Zi /Od /I "../libmp3lame" /I "../include" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_BLADEDLL" /D "TAKEHIRO_IEEE754_HACK" /YX /FD /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o NUL /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o NUL /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /out:"Debug\lame_enc.dll" /pdbtype:sept
# SUBTRACT LINK32 /map

!ENDIF 

# Begin Target

# Name "MP3EncDll - Win32 Release"
# Name "MP3EncDll - Win32 Debug"
# Begin Group "Source"

# PROP Default_Filter "c"
# Begin Source File

SOURCE=..\libmp3lame\bitstream.c
# End Source File
# Begin Source File

SOURCE=.\BladeMP3EncDLL.c
# End Source File
# Begin Source File

SOURCE=.\BladeMP3EncDLL.def
# End Source File
# Begin Source File

SOURCE=..\libmp3lame\encoder.c
# End Source File
# Begin Source File

SOURCE=..\libmp3lame\fft.c
# End Source File
# Begin Source File

SOURCE=..\libmp3lame\id3tag.c
# End Source File
# Begin Source File

SOURCE=..\libmp3lame\lame.c
# End Source File
# Begin Source File

SOURCE=..\libmp3lame\mpglib_interface.c
# End Source File
# Begin Source File

SOURCE=..\libmp3lame\newmdct.c
# End Source File
# Begin Source File

SOURCE=..\libmp3lame\psymodel.c
# End Source File
# Begin Source File

SOURCE=..\libmp3lame\quantize.c
# End Source File
# Begin Source File

SOURCE=..\libmp3lame\quantize_pvt.c
# End Source File
# Begin Source File

SOURCE=..\libmp3lame\reservoir.c
# End Source File
# Begin Source File

SOURCE=..\libmp3lame\tables.c
# End Source File
# Begin Source File

SOURCE=..\libmp3lame\takehiro.c
# End Source File
# Begin Source File

SOURCE=..\libmp3lame\util.c
# End Source File
# Begin Source File

SOURCE=..\libmp3lame\vbrquantize.c
# End Source File
# Begin Source File

SOURCE=..\libmp3lame\VbrTag.c
# End Source File
# Begin Source File

SOURCE=..\libmp3lame\version.c
# End Source File
# End Group
# Begin Group "Include"

# PROP Default_Filter "h"
# Begin Source File

SOURCE=..\libmp3lame\bitstream.h
# End Source File
# Begin Source File

SOURCE=.\BladeMP3EncDLL.h
# End Source File
# Begin Source File

SOURCE=..\libmp3lame\encoder.h
# End Source File
# Begin Source File

SOURCE=..\libmp3lame\fft.h
# End Source File
# Begin Source File

SOURCE=..\libmp3lame\l3side.h
# End Source File
# Begin Source File

SOURCE="..\libmp3lame\lame-analysis.h"
# End Source File
# Begin Source File

SOURCE=..\libmp3lame\machine.h
# End Source File
# Begin Source File

SOURCE=..\libmp3lame\newmdct.h
# End Source File
# Begin Source File

SOURCE=..\libmp3lame\psymodel.h
# End Source File
# Begin Source File

SOURCE=..\libmp3lame\quantize.h
# End Source File
# Begin Source File

SOURCE=..\libmp3lame\quantize_pvt.h
# End Source File
# Begin Source File

SOURCE=..\libmp3lame\reservoir.h
# End Source File
# Begin Source File

SOURCE=..\libmp3lame\tables.h
# End Source File
# Begin Source File

SOURCE=..\libmp3lame\util.h
# End Source File
# Begin Source File

SOURCE=..\libmp3lame\VbrTag.h
# End Source File
# Begin Source File

SOURCE=..\libmp3lame\version.h
# End Source File
# End Group
# End Target
# End Project
