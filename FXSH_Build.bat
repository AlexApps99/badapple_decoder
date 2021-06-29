@echo off
rem Do not edit! This batch file is created by CASIO fx-9860G SDK.


if exist BADAPPLE.G1A  del BADAPPLE.G1A

cd debug
if exist FXADDINror.bin  del FXADDINror.bin
"C:\Users\LXB\Desktop\CASIO\SDK\OS\SH\Bin\Hmake.exe" Addin.mak
cd ..
if not exist debug\FXADDINror.bin  goto error

"C:\Users\LXB\Desktop\CASIO\SDK\Tools\MakeAddinHeader363.exe" "C:\Users\LXB\Desktop\touhou\BadApple"
if not exist BADAPPLE.G1A  goto error
echo Build has completed.
goto end

:error
echo Build was not successful.

:end

