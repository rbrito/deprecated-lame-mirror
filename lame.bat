@echo off
rem  ------------------------------------------
rem  PURPOSE:
rem  put this Batch-Command on your Desktop, 
rem  so you can drag and drop wave files on it
rem  and LAME will encode them to mp3 format.
rem  NOTE:
rem  DOS has only 8+3 file names!
rem  ------------------------------------------
rem                       C2000 Robert Hegemann
rem                      Robert.Hegemann@gmx.de
rem  ------------------------------------------
rem  please set LAME and LAMEOPTS
rem  LAME - where the executeable is
rem  OPTS - options you like LAME to use

	set LAME=lame.exe
	set OPTS=--preset cd

rem  ------------------------------------------

	set thecmd=%LAME% %OPTS%
:processArgs
	if "%1"=="" goto endmark
	for %%f in (%1) do %thecmd% %%f
	if errorlevel 1 goto errormark
	shift
	goto processArgs
:errormark
	echo.
	echo.
	echo ERROR processing %1
	echo. 
:endmark
rem
rem	finished
rem