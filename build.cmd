@echo off
setlocal
set "VCPKG_ROOT=C:\ProjectCodex\vcpkg"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\Preview\Common7\Tools\VsDevCmd.bat" -arch=amd64
cmake --preset release
cmake --build --preset release
C:\ProjectCodex\bin\Release\vampire.exe
