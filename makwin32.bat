@echo off

echo Checking environment...
if exist \msdev\bin\vcvars32 if "%INCLUDE%"=="" call \msdev\bin\vcvars32
if "%INCLUDE%"=="" call vcvars32

echo Configuring...
if not exist make.bat copy makwin32.bat make.bat
if not exist ctags.mak copy oswin32\ctags.mak
if not exist elvis.mak copy oswin32\elvis.mak
if not exist elvisutl.mak copy oswin32\elvisutl.mak
if not exist fmt.mak copy oswin32\fmt.mak
if not exist ref.mak copy oswin32\ref.mak
if not exist winelvis.mak copy oswin32\winelvis.mak
if not exist wintags.mak copy oswin32\wintags.mak
if not exist config.h copy oswin32\osconfig.h config.h
if not exist WinRel\nul md WinRel

echo Compiling text-mode utilities...
nmake /nologo /s /f elvis.mak "CFG=elvis - Win32 Release"
if errorlevel 1 goto Fail
nmake /nologo /s /f elvisutl.mak
if errorlevel 1 goto Fail

echo Compiling graphical utilities...
nmake /nologo /s /f winelvis.mak CFG="WinElvis - Win32 Release"
if errorlevel 1 goto Fail
nmake /nologo /s /f wintags.mak CFG="WinTags - Win32 Release"
if errorlevel 1 goto Fail

:Fail
