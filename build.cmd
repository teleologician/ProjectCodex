@echo off
setlocal
set "ROOT=C:\ProjectCodex"
if not exist "%ROOT%\vcpkg\vcpkg.exe" goto :no_vcpkg

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\Preview\Common7\Tools\VsDevCmd.bat" -arch=amd64
set "VCPKG_ROOT=%ROOT%\vcpkg"
set "VCPKG_OVERLAY_TRIPLETS=%ROOT%\cmake\triplets"
set "VCPKG_DEFAULT_TRIPLET=x64-windows"
set "VCPKG_TARGET_TRIPLET=x64-windows"
pushd "%ROOT%"
set "VCPKG_DEFAULT_TRIPLET=x64-windows"
"%VCPKG_ROOT%\vcpkg.exe" install --triplet x64-windows --x-manifest-root="%ROOT%" --vcpkg-root="%VCPKG_ROOT%" --overlay-triplets="%VCPKG_OVERLAY_TRIPLETS%"
cmake --preset vs2022
cmake --build --preset release
popd
"%ROOT%\bin\Release\vampire.exe"
exit /b 0

:no_vcpkg
echo vcpkg.exe not found at %VCPKG_ROOT%.
exit /b 1
