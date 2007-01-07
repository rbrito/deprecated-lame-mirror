# Microsoft Developer Studio Project File - Name="libmp3lame DLL" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** NICHT BEARBEITEN **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=libmp3lame DLL - Win32 Release
!MESSAGE Dies ist kein gültiges Makefile. Zum Erstellen dieses Projekts mit NMAKE
!MESSAGE verwenden Sie den Befehl "Makefile exportieren" und führen Sie den Befehl
!MESSAGE 
!MESSAGE NMAKE /f "libmp3lame_dll_vc6.mak".
!MESSAGE 
!MESSAGE Sie können beim Ausführen von NMAKE eine Konfiguration angeben
!MESSAGE durch Definieren des Makros CFG in der Befehlszeile. Zum Beispiel:
!MESSAGE 
!MESSAGE NMAKE /f "libmp3lame_dll_vc6.mak" CFG="libmp3lame DLL - Win32 Release"
!MESSAGE 
!MESSAGE Für die Konfiguration stehen zur Auswahl:
!MESSAGE 
!MESSAGE "libmp3lame DLL - Win32 Release" (basierend auf  "Win32 (x86) Dynamic-Link Library")
!MESSAGE "libmp3lame DLL - Win32 Debug" (basierend auf  "Win32 (x86) Dynamic-Link Library")
!MESSAGE "libmp3lame DLL - Win32 Release NASM" (basierend auf  "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libmp3lame DLL - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "libmp3la"
# PROP BASE Intermediate_Dir "libmp3la"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\output\Release\libmp3lame"
# PROP Intermediate_Dir "..\obj\Release\libmp3lame"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
LIB32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /Ob2 /D "WIN32" /D "HAVE_CONFIG_H" /D "NDEBUG" /D "_WINDOWS" /YX /Gs1024 /FD /GAy /QIfdiv /QI0f /c
# ADD CPP /nologo /MD /W3 /O2 /Ob2 /I "../" /I "../mpglib" /I "../include" /I ".." /D "NDEBUG" /D "_WINDOWS" /D "HAVE_MPGLIB" /D "WIN32" /D "HAVE_CONFIG_H" /Gs1024 /FD /GAy /QIfdiv /QI0f /c
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 mpglib.lib libmp3lame-static.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /pdb:none /machine:I386 /def:"..\include\lame.def" /force /out:"..\output\Release\libmp3lame\libmp3lame-dynamic.dll" /libpath:"..\output\Release\mpglib" /libpath:"..\output\Release\libmp3lame" /opt:NOWIN98
# SUBTRACT LINK32 /nodefaultlib

!ELSEIF  "$(CFG)" == "libmp3lame DLL - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\output\Debug\libmp3lame"
# PROP Intermediate_Dir "..\obj\Debug\libmp3lame"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
LIB32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /Z7 /Od /D "WIN32" /D "HAVE_CONFIG_H" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /W3 /Zi /Od /I "../" /I "../mpglib" /I "../include" /I ".." /D "_DEBUG" /D "_WINDOWS" /D "HAVE_MPGLIB" /D "WIN32" /D "HAVE_CONFIG_H" /FD /c
# SUBTRACT CPP /Fr /YX
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 mpglib.lib libmp3lame-static.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /def:"..\include\lame.def" /force /out:"..\output\Debug\libmp3lame\libmp3lame-dynamic.dll" /libpath:"..\output\Debug\mpglib" /libpath:"..\output\Debug\libmp3lame" /opt:NOWIN98
# SUBTRACT LINK32 /pdb:none /map /nodefaultlib

!ELSEIF  "$(CFG)" == "libmp3lame DLL - Win32 Release NASM"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "libmp3la"
# PROP BASE Intermediate_Dir "libmp3la"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\output\Release_NASM\libmp3lame"
# PROP Intermediate_Dir "..\obj\Release_NASM\libmp3lame"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
LIB32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /Ob2 /I "../" /I "../mpglib" /I "../include" /I ".." /D "NDEBUG" /D "_WINDOWS" /D "HAVE_MPGLIB" /D "WIN32" /D "HAVE_CONFIG_H" /Gs1024 /FD /GAy /QIfdiv /QI0f /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /W3 /O2 /Ob2 /I "../" /I "../mpglib" /I "../include" /I ".." /D "NDEBUG" /D "_WINDOWS" /D "HAVE_MPGLIB" /D "WIN32" /D "HAVE_CONFIG_H" /D "HAVE_NASM" /D "MMX_choose_table" /D "_CRT_SECURE_NO_DEPRECATE" /Gs1024 /FD /GAy /QIfdiv /QI0f /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 mpglib.lib libmp3lame-static.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /pdb:none /machine:I386 /def:"..\include\lame.def" /force /out:"..\output\Release_NASM\libmp3lame\libmp3lame-dynamic.dll" /libpath:"..\output\Release_NASM\mpglib" /libpath:"..\output\Release_NASM\libmp3lame" /opt:NOWIN98
# SUBTRACT LINK32 /nodefaultlib

!ENDIF 

# Begin Target

# Name "libmp3lame DLL - Win32 Release"
# Name "libmp3lame DLL - Win32 Debug"
# Name "libmp3lame DLL - Win32 Release NASM"
# Begin Source File

SOURCE=..\include\lame.def
# PROP Exclude_From_Build 1
# End Source File
# End Target
# End Project
