# Microsoft Developer Studio Project File - Name="MP3x" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 5.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=MP3X - WIN32 RELEASE
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "MP3x.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "MP3x.mak" CFG="MP3X - WIN32 RELEASE"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "MP3x - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "MP3x - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=xicl.exe
RSC=rc.exe

!IF  "$(CFG)" == "MP3x - Win32 Release"

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
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /Ox /Ot /Og /I "./WinGTK/gtk-plus" /I "./WinGTK/glib-1.2" /I "../include" /I "../" /I "../mp3x" /I "../frontend" /I "./WinGtk/src/gtk+" /I "./WinGtk/src/glib" /I "./WinGtk/src/gtk+/gdk" /D "NDEBUG" /D "LAMEPARSE" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "HAVEGTK" /D "HAVEMPGLIB" /D "LAMESNDFILE" /D "BRHIST" /D "NOTERMCAP" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib glib-1.3.lib gdk-1.3.lib gtk-1.3.lib /nologo /subsystem:console /profile /map /machine:I386 /libpath:"./WinGtk/src/gtk+/gtk" /libpath:"./WinGtk/src/gtk+/gdk" /libpath:"./WinGtk/src/glib"

!ELSEIF  "$(CFG)" == "MP3x - Win32 Debug"

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
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /Gm /GX /Zi /Od /I "./WinGtk/src/gtk+" /I "./WinGtk/src/glib" /I "./WinGtk/src/gtk+/gdk" /I "../include" /I "../" /I "../mp3x" /I "../frontend" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "HAVEGTK" /D "HAVEMPGLIB" /D "LAMESNDFILE" /D "BRHIST" /D "NOTERMCAP" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib glib-1.3.lib gdk-1.3.lib gtk-1.3.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept /libpath:"./WinGtk/src/gtk+/gtk" /libpath:"./WinGtk/src/gtk+/gdk" /libpath:"./WinGtk/src/glib"

!ENDIF 

# Begin Target

# Name "MP3x - Win32 Release"
# Name "MP3x - Win32 Debug"
# Begin Group "Source"

# PROP Default_Filter "c"
# Begin Source File

SOURCE=..\frontend\brhist.c
# End Source File
# Begin Source File

SOURCE=..\frontend\get_audio.c
# End Source File
# Begin Source File

SOURCE=.\gpkplotting.c
# End Source File
# Begin Source File

SOURCE=.\gtkanal.c
# End Source File
# Begin Source File

SOURCE=..\frontend\ieeefloat.c
# End Source File
# Begin Source File

SOURCE=..\frontend\lametime.c
# End Source File
# Begin Source File

SOURCE=.\mp3x.c
# End Source File
# Begin Source File

SOURCE=..\frontend\parse.c
# End Source File
# Begin Source File

SOURCE=..\frontend\portableio.c
# End Source File
# Begin Source File

SOURCE=..\frontend\timestatus.c
# End Source File
# End Group
# Begin Group "Include"

# PROP Default_Filter "h"
# Begin Source File

SOURCE=..\frontend\brhist.h
# End Source File
# Begin Source File

SOURCE=..\frontend\get_audio.h
# End Source File
# Begin Source File

SOURCE=.\gpkplotting.h
# End Source File
# Begin Source File

SOURCE=..\frontend\ieeefloat.h
# End Source File
# Begin Source File

SOURCE=..\frontend\lametime.h
# End Source File
# Begin Source File

SOURCE=..\frontend\parse.h
# End Source File
# Begin Source File

SOURCE=..\frontend\portableio.h
# End Source File
# Begin Source File

SOURCE=..\frontend\timestatus.h
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
