# Microsoft Developer Studio Project File - Name="lamelib" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 5.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=lamelib - Win32 Debug GTK
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "lamelib.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "lamelib.mak" CFG="lamelib - Win32 Debug GTK"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "lamelib - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "lamelib - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "lamelib - Win32 Debug GTK" (based on "Win32 (x86) Static Library")
!MESSAGE "lamelib - Win32 Release GTK" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=xicl.exe

!IF  "$(CFG)" == "lamelib - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "./include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "lamelib - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /Z7 /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /Z7 /Od /I "./include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "lamelib - Win32 Debug GTK"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "lamelib_"
# PROP BASE Intermediate_Dir "lamelib_"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "DebugGTK"
# PROP Intermediate_Dir "DebugGTK"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /Z7 /Od /I "./include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /Z7 /Od /I "./include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "HAVEGTK" /YX /FD /c
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "lamelib - Win32 Release GTK"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "lamelib0"
# PROP BASE Intermediate_Dir "lamelib0"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "ReleaseGTK"
# PROP Intermediate_Dir "ReleaseGTK"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /I "./include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "./include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "HAVEGTK" /YX /FD /c
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "lamelib - Win32 Release"
# Name "lamelib - Win32 Debug"
# Name "lamelib - Win32 Debug GTK"
# Name "lamelib - Win32 Release GTK"
# Begin Group "Source"

# PROP Default_Filter "c"
# Begin Source File

SOURCE=.\bitstream.c
# End Source File
# Begin Source File

SOURCE=.\encoder.c
# End Source File
# Begin Source File

SOURCE=.\fft.c
# End Source File
# Begin Source File

SOURCE=.\id3tag.c
# End Source File
# Begin Source File

SOURCE=.\lame.c
# End Source File
# Begin Source File

SOURCE=.\newmdct.c
# End Source File
# Begin Source File

SOURCE=.\psymodel.c
# End Source File
# Begin Source File

SOURCE=.\quantize.c
# End Source File
# Begin Source File

SOURCE=.\quantize_pvt.c
# End Source File
# Begin Source File

SOURCE=.\reservoir.c
# End Source File
# Begin Source File

SOURCE=.\tables.c
# End Source File
# Begin Source File

SOURCE=.\takehiro.c
# End Source File
# Begin Source File

SOURCE=.\util.c
# End Source File
# Begin Source File

SOURCE=.\vbrquantize.c
# End Source File
# Begin Source File

SOURCE=.\VbrTag.c
# End Source File
# Begin Source File

SOURCE=.\version.c
# End Source File
# End Group
# Begin Group "Include"

# PROP Default_Filter "h"
# Begin Source File

SOURCE=.\bitstream.h
# End Source File
# Begin Source File

SOURCE=.\confdefs.h
# End Source File
# Begin Source File

SOURCE=.\encoder.h
# End Source File
# Begin Source File

SOURCE=.\fft.h
# End Source File
# Begin Source File

SOURCE=.\l3side.h
# End Source File
# Begin Source File

SOURCE=.\machine.h
# End Source File
# Begin Source File

SOURCE=.\newmdct.h
# End Source File
# Begin Source File

SOURCE=.\psymodel.h
# End Source File
# Begin Source File

SOURCE=.\quantize.h
# End Source File
# Begin Source File

SOURCE=.\quantize_pvt.h
# End Source File
# Begin Source File

SOURCE=.\reservoir.h
# End Source File
# Begin Source File

SOURCE=.\tables.h
# End Source File
# Begin Source File

SOURCE=.\util.h
# End Source File
# Begin Source File

SOURCE=.\VbrTag.h
# End Source File
# Begin Source File

SOURCE=.\version.h
# End Source File
# End Group
# End Target
# End Project
