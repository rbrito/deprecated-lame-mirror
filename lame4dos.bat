@echo off
rem  ---------------------------------------------
rem  PURPOSE:
rem  - put this Batch-Command on your Desktop, 
rem    so you can drag and drop wave files on it
rem    and LAME will encode them to mp3 format.
rem    NOTE: DOS has only 8+3 file names!
rem    (if someone knows how to configure Windows
rem     to hand over long file names with "drag 
rem     and drop" to batch files, please tell me)
rem  - put this Batch-Command in a place mentioned
rem    in your PATH environment, start the DOS-BOX
rem    and change to a directory where your wave 
rem    files are located. the following line will
rem    encode all your wave files to mp3
rem     "lame.bat *.wav"
rem  ---------------------------------------------
rem                         C2000  Robert Hegemann
rem                         Robert.Hegemann@gmx.de
rem  ---------------------------------------------
rem  Changes to support long filenames using 4DOS
rem  by Alexander Stumpf <dropdachalupa@gmx.net>
rem  ---------------------------------------------
rem  please set LAME and LAMEOPTS
rem  LAME - where the executeable is
rem  OPTS - options you like LAME to use

        set LAME=lame.exe
	set OPTS=-v --preset hifi -b 80 --abr 148

rem  ---------------------------------------------

	set thecmd=%LAME% %OPTS%
:processArgs
	if "%1"=="" goto endmark
	for %%f in (%1) do (echo %thecmd% %@if[%@ext[%%f]==mp3,--mp3input ]%%f^ren %%f.mp3 "%@lfn[%%f].mp3")
	if errorlevel 1 goto errormark
	shift
	goto processArgs
	ren *.wav.mp3 *.mp3 > NUL
:errormark
	echo.
	echo.
	echo ERROR processing %1
	echo. 
:endmark
rem
rem	finished
rem
