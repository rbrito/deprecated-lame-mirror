# Microsoft Developer Studio Project File - Name="lameACM" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 5.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=lameACM - Win32 Debug NASM
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "lameACM.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "lameACM.mak" CFG="lameACM - Win32 Debug NASM"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "lameACM - Win32 Release" (based on\
 "Win32 (x86) Dynamic-Link Library")
!MESSAGE "lameACM - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "lameACM - Win32 Debug NASM" (based on\
 "Win32 (x86) Dynamic-Link Library")
!MESSAGE "lameACM - Win32 Release NASM" (based on\
 "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "lameACM - Win32 Release"

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
# ADD CPP /nologo /Zp2 /MT /W3 /GX /Ox /Ot /Og /Ob2 /I "../libmp3lame" /I "../include" /I ".." /I "../.." /I "../mpglib" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_BLADEDLL" /D "TAKEHIRO_IEEE754_HACK" /D "HAVE_CONFIG_H" /D "HAVE_MPGLIB" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o /win32 "NUL"
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o /win32 "NUL"
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 winmm.lib advapi32.lib user32.lib kernel32.lib libc.lib /nologo /subsystem:windows /dll /map /machine:I386 /nodefaultlib /out:"Release\lameACM.acm"

!ELSEIF  "$(CFG)" == "lameACM - Win32 Debug"

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
# ADD CPP /nologo /Zp2 /MTd /W3 /GX /Od /I "../libmp3lame" /I "../include" /I ".." /I "../.." /I "../mpglib" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_BLADEDLL" /D "TAKEHIRO_IEEE754_HACK" /D "HAVE_CONFIG_H" /D "HAVE_MPGLIB" /YX /FD /ZI /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o /win32 "NUL"
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o /win32 "NUL"
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 winmm.lib advapi32.lib user32.lib kernel32.lib libcd.lib /nologo /subsystem:windows /dll /debug /machine:I386 /nodefaultlib /out:"Debug\lameACM.acm" /pdbtype:sept
# SUBTRACT LINK32 /map
# Begin Special Build Tool
SOURCE=$(InputPath)
PostBuild_Cmds=copy Debug\lameACM.acm e:\winnt\system32\lameACM.acm
# End Special Build Tool

!ELSEIF  "$(CFG)" == "lameACM - Win32 Debug NASM"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug_NASM"
# PROP BASE Intermediate_Dir "Debug_NASM"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug_NASM"
# PROP Intermediate_Dir "Debug_NASM"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /Zp2 /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /Zp2 /MTd /W3 /GX /Od /I "../libmp3lame" /I "../include" /I ".." /I "../.." /I "../mpglib" /D "_DEBUG" /D "HAVE_NASM" /D "MMX_choose_table" /D "WIN32" /D "_WINDOWS" /D "_BLADEDLL" /D "TAKEHIRO_IEEE754_HACK" /D "HAVE_CONFIG_H" /D "HAVE_MPGLIB" /YX /FD /ZI /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o /win32 "NUL"
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o /win32 "NUL"
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# SUBTRACT BASE LINK32 /map
# ADD LINK32 winmm.lib advapi32.lib user32.lib kernel32.lib libcd.lib ADbg/Debug/adbg.lib ../mpglib/Debug/mpglib.lib /nologo /subsystem:windows /dll /debug /machine:I386 /nodefaultlib /out:"Debug_NASM\lameACM.acm" /pdbtype:sept
# SUBTRACT LINK32 /map
# Begin Special Build Tool
SOURCE=$(InputPath)
PostBuild_Cmds=copy Debug_NASM\lameACM.acm d:\winnt\system32\lameACM.acm
# End Special Build Tool

!ELSEIF  "$(CFG)" == "lameACM - Win32 Release NASM"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release_NASM"
# PROP BASE Intermediate_Dir "Release_NASM"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release_NASM"
# PROP Intermediate_Dir "Release_NASM"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /Zp2 /MT /W3 /GX /Ox /Ot /Og /Ob2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /Zp2 /MT /W3 /GX /Ox /Ot /Og /Ob2 /I "../libmp3lame" /I "../include" /I ".." /I "../.." /I "../mpglib" /D "NDEBUG" /D "HAVE_NASM" /D "MMX_choose_table" /D "WIN32" /D "_WINDOWS" /D "_BLADEDLL" /D "TAKEHIRO_IEEE754_HACK" /D "HAVE_CONFIG_H" /D "HAVE_MPGLIB" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o /win32 "NUL"
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o /win32 "NUL"
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /map /machine:I386
# ADD LINK32 winmm.lib advapi32.lib user32.lib kernel32.lib libc.lib ADbg\Release\adbg.lib /nologo /subsystem:windows /dll /map /machine:I386 /nodefaultlib /out:"Release_NASM\lameACM.acm"

!ENDIF 

# Begin Target

# Name "lameACM - Win32 Release"
# Name "lameACM - Win32 Debug"
# Name "lameACM - Win32 Debug NASM"
# Name "lameACM - Win32 Release NASM"
# Begin Group "Source"

# PROP Default_Filter "c;cpp"
# Begin Source File

SOURCE=.\ACM.cpp
# End Source File
# Begin Source File

SOURCE=.\ACMStream.cpp
# End Source File
# Begin Source File

SOURCE=..\libmp3lame\bitstream.c
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

SOURCE=.\lameACM.def
# End Source File
# Begin Source File

SOURCE=.\main.cpp
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

SOURCE=..\libmp3lame\set_get.c
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

SOURCE=.\ACM.h
# End Source File
# Begin Source File

SOURCE=.\ACMStream.h
# End Source File
# Begin Source File

SOURCE=.\adebug.h
# End Source File
# Begin Source File

SOURCE=..\libmp3lame\bitstream.h
# End Source File
# Begin Source File

SOURCE=.\BladelameACM.h
# End Source File
# Begin Source File

SOURCE=..\configMS.h

!IF  "$(CFG)" == "lameACM - Win32 Release"

# Begin Custom Build
InputPath=..\configMS.h

"..\config.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy ..\configMS.h ..\config.h

# End Custom Build

!ELSEIF  "$(CFG)" == "lameACM - Win32 Debug"

# Begin Custom Build
InputPath=..\configMS.h

"..\config.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy ..\configMS.h ..\config.h

# End Custom Build

!ELSEIF  "$(CFG)" == "lameACM - Win32 Debug NASM"

# Begin Custom Build
InputPath=..\configMS.h

"..\config.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy ..\configMS.h ..\config.h

# End Custom Build

!ELSEIF  "$(CFG)" == "lameACM - Win32 Release NASM"

# Begin Custom Build
InputPath=..\configMS.h

"..\config.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy ..\configMS.h ..\config.h

# End Custom Build

!ENDIF 

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
# Begin Group "Asm"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\libmp3lame\i386\choose_table.nas

!IF  "$(CFG)" == "lameACM - Win32 Release"

!ELSEIF  "$(CFG)" == "lameACM - Win32 Debug"

!ELSEIF  "$(CFG)" == "lameACM - Win32 Debug NASM"

# Begin Custom Build - Assembling
InputDir="\users\slhomme\perso\lame\lame-3.90\libmp3lame\i386"
OutDir=.\Debug_NASM
InputPath=..\libmp3lame\i386\choose_table.nas
InputName=choose_table

"$(OutDir)/$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -f win32 -i $(InputDir)/ -DWIN32 $(InputPath) -o\
       $(OutDir)/$(InputName).obj

# End Custom Build

!ELSEIF  "$(CFG)" == "lameACM - Win32 Release NASM"

# Begin Custom Build - Assembling
InputDir="\users\slhomme\perso\lame\lame-3.90\libmp3lame\i386"
OutDir=.\Release_NASM
InputPath=..\libmp3lame\i386\choose_table.nas
InputName=choose_table

"$(OutDir)/$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -f win32 -i $(InputDir)/ -DWIN32 $(InputPath) -o\
       $(OutDir)/$(InputName).obj

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\libmp3lame\i386\cpu_feat.nas

!IF  "$(CFG)" == "lameACM - Win32 Release"

!ELSEIF  "$(CFG)" == "lameACM - Win32 Debug"

# Begin Custom Build - Assembling
InputDir="\users\slhomme\perso\lame\lame-3.90\libmp3lame\i386"
OutDir=.\Debug
InputPath=..\libmp3lame\i386\cpu_feat.nas
InputName=cpu_feat

"$(OutDir)/$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -f win32 -i $(InputDir)/ -DWIN32 $(InputPath) -o\
       $(OutDir)/$(InputName).obj

# End Custom Build

!ELSEIF  "$(CFG)" == "lameACM - Win32 Debug NASM"

# Begin Custom Build - Assembling
InputDir="\users\slhomme\perso\lame\lame-3.90\libmp3lame\i386"
OutDir=.\Debug_NASM
InputPath=..\libmp3lame\i386\cpu_feat.nas
InputName=cpu_feat

"$(OutDir)/$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -f win32 -i $(InputDir)/ -DWIN32 $(InputPath) -o\
       $(OutDir)/$(InputName).obj

# End Custom Build

!ELSEIF  "$(CFG)" == "lameACM - Win32 Release NASM"

# Begin Custom Build - Assembling
InputDir="\users\slhomme\perso\lame\lame-3.90\libmp3lame\i386"
OutDir=.\Release_NASM
InputPath=..\libmp3lame\i386\cpu_feat.nas
InputName=cpu_feat

"$(OutDir)/$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -f win32 -i $(InputDir)/ -DWIN32 $(InputPath) -o\
       $(OutDir)/$(InputName).obj

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\libmp3lame\i386\fft.nas
# End Source File
# Begin Source File

SOURCE=..\libmp3lame\i386\fft3dn.nas

!IF  "$(CFG)" == "lameACM - Win32 Release"

!ELSEIF  "$(CFG)" == "lameACM - Win32 Debug"

!ELSEIF  "$(CFG)" == "lameACM - Win32 Debug NASM"

# Begin Custom Build - Assembling
InputDir="\users\slhomme\perso\lame\lame-3.90\libmp3lame\i386"
OutDir=.\Debug_NASM
InputPath=..\libmp3lame\i386\fft3dn.nas
InputName=fft3dn

"$(OutDir)/$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -f win32 -i $(InputDir)/ -DWIN32 $(InputPath) -o\
       $(OutDir)/$(InputName).obj

# End Custom Build

!ELSEIF  "$(CFG)" == "lameACM - Win32 Release NASM"

# Begin Custom Build - Assembling
InputDir="\users\slhomme\perso\lame\lame-3.90\libmp3lame\i386"
OutDir=.\Release_NASM
InputPath=..\libmp3lame\i386\fft3dn.nas
InputName=fft3dn

"$(OutDir)/$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -f win32 -i $(InputDir)/ -DWIN32 $(InputPath) -o\
       $(OutDir)/$(InputName).obj

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\libmp3lame\i386\fftfpu.nas
# End Source File
# Begin Source File

SOURCE=..\libmp3lame\i386\fftsse.nas
# End Source File
# Begin Source File

SOURCE=..\libmp3lame\i386\ffttbl.nas
# End Source File
# Begin Source File

SOURCE=..\libmp3lame\i386\scalar.nas

!IF  "$(CFG)" == "lameACM - Win32 Release"

!ELSEIF  "$(CFG)" == "lameACM - Win32 Debug"

# Begin Custom Build - Assembling
InputDir="\users\slhomme\perso\lame\lame-3.90\libmp3lame\i386"
OutDir=.\Debug
InputPath=..\libmp3lame\i386\scalar.nas
InputName=scalar

"$(OutDir)/$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -f win32 -i $(InputDir)/ -DWIN32 $(InputPath) -o\
       $(OutDir)/$(InputName).obj

# End Custom Build

!ELSEIF  "$(CFG)" == "lameACM - Win32 Debug NASM"

# Begin Custom Build - Assembling
InputDir="\users\slhomme\perso\lame\lame-3.90\libmp3lame\i386"
OutDir=.\Debug_NASM
InputPath=..\libmp3lame\i386\scalar.nas
InputName=scalar

"$(OutDir)/$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -f win32 -i $(InputDir)/ -DWIN32 $(InputPath) -o\
       $(OutDir)/$(InputName).obj

# End Custom Build

!ELSEIF  "$(CFG)" == "lameACM - Win32 Release NASM"

# Begin Custom Build - Assembling
InputDir="\users\slhomme\perso\lame\lame-3.90\libmp3lame\i386"
OutDir=.\Release_NASM
InputPath=..\libmp3lame\i386\scalar.nas
InputName=scalar

"$(OutDir)/$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -f win32 -i $(InputDir)/ -DWIN32 $(InputPath) -o\
       $(OutDir)/$(InputName).obj

# End Custom Build

!ENDIF 

# End Source File
# End Group
# Begin Group "Resource"

# PROP Default_Filter "rc"
# Begin Source File

SOURCE=.\acm.rc

!IF  "$(CFG)" == "lameACM - Win32 Release"

!ELSEIF  "$(CFG)" == "lameACM - Win32 Debug"

!ELSEIF  "$(CFG)" == "lameACM - Win32 Debug NASM"

!ELSEIF  "$(CFG)" == "lameACM - Win32 Release NASM"

!ENDIF 

# End Source File
# End Group
# Begin Group "Install"

# PROP Default_Filter "inf;acm"
# Begin Source File

SOURCE=.\LameACM.inf
# End Source File
# End Group
# Begin Source File

SOURCE=.\lame.ico
# End Source File
# Begin Source File

SOURCE=.\readme.txt
# End Source File
# Begin Source File

SOURCE=.\TODO
# End Source File
# End Target
# End Project
