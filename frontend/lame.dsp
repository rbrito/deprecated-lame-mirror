# Microsoft Developer Studio Project File - Name="lame" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 5.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=lame - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "lame.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "lame.mak" CFG="lame - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "lame - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "lame - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "lame - Win32 Release NASM" (based on\
 "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=xicl.exe
RSC=rc.exe

!IF  "$(CFG)" == "lame - Win32 Release"

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
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "HAVE_CONFIG_H" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /O2 /I "./WinGTK/gtk-plus" /I "./WinGTK/glib-1.2" /I "../" /I "../mp3x" /I "../frontend" /I "../include" /I ".." /D "NDEBUG" /D "LAMEPARSE" /D "_CONSOLE" /D "_MBCS" /D "HAVE_MPGLIB" /D "LAMESNDFILE" /D "BRHIST" /D "WIN32" /D "HAVE_CONFIG_H" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# SUBTRACT LINK32 /profile /map

!ELSEIF  "$(CFG)" == "lame - Win32 Debug"

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
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "HAVE_CONFIG_H" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /Od /I "../" /I "../mp3x" /I "../frontend" /I "../include" /I ".." /D "_DEBUG" /D "LAMEPARSE" /D "_CONSOLE" /D "_MBCS" /D "HAVEMPGLIB" /D "LAMESNDFILE" /D "BRHIST" /D "WIN32" /D "HAVE_CONFIG_H" /YX /FD /ZI /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ELSEIF  "$(CFG)" == "lame - Win32 Release NASM"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "lame___W"
# PROP BASE Intermediate_Dir "lame___W"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release_NASM"
# PROP Intermediate_Dir "Release_NASM"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /O2 /I "./WinGTK/gtk-plus" /I "./WinGTK/glib-1.2" /I "../" /I "../mp3x" /I "../frontend" /I "../include" /I ".." /D "NDEBUG" /D "LAMEPARSE" /D "_CONSOLE" /D "_MBCS" /D "HAVE_MPGLIB" /D "LAMESNDFILE" /D "BRHIST" /D "WIN32" /D "HAVE_CONFIG_H" /FD /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /W3 /O2 /I "./WinGTK/gtk-plus" /I "./WinGTK/glib-1.2" /I "../" /I "../mp3x" /I "../frontend" /I "../include" /I ".." /D "NDEBUG" /D "LAMEPARSE" /D "_CONSOLE" /D "_MBCS" /D "HAVE_MPGLIB" /D "LAMESNDFILE" /D "BRHIST" /D "WIN32" /D "HAVE_CONFIG_H" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# SUBTRACT BASE LINK32 /profile /map
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# SUBTRACT LINK32 /profile /map

!ENDIF 

# Begin Target

# Name "lame - Win32 Release"
# Name "lame - Win32 Debug"
# Name "lame - Win32 Release NASM"
# Begin Group "Source"

# PROP Default_Filter "c"
# Begin Source File

SOURCE=.\brhist.c
# End Source File
# Begin Source File

SOURCE=.\get_audio.c
# End Source File
# Begin Source File

SOURCE=.\lametime.c
# End Source File
# Begin Source File

SOURCE=.\main.c
# End Source File
# Begin Source File

SOURCE=.\parse.c
# End Source File
# Begin Source File

SOURCE=.\portableio.c
# End Source File
# Begin Source File

SOURCE=.\timestatus.c
# End Source File
# End Group
# Begin Group "Include"

# PROP Default_Filter "h"
# Begin Source File

SOURCE=.\brhist.h
# End Source File
# Begin Source File

SOURCE=..\configMS.h
# End Source File
# Begin Source File

SOURCE=.\get_audio.h
# End Source File
# Begin Source File

SOURCE=.\lametime.h
# End Source File
# Begin Source File

SOURCE=.\main.h
# End Source File
# Begin Source File

SOURCE=.\parse.h
# End Source File
# Begin Source File

SOURCE=.\portableio.h
# End Source File
# Begin Source File

SOURCE=.\rtp.h
# End Source File
# Begin Source File

SOURCE=.\timestatus.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\Readme
# End Source File
# Begin Source File

SOURCE=.\Readme.wingtk
# End Source File
# End Target
# End Project
