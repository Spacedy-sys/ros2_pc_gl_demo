@echo off
REM ---------------------------------------------------------------------------
REM Windows local verification build (no ROS needed).
REM Uses the VS-bundled vcvars64 + CMake/Ninja to build the windows_preview
REM target. Output: build\pc_gl_viewer_preview.exe
REM
REM Usage: scripts\build_windows.cmd
REM Needs: Visual Studio 2019/2022 with the "Desktop development with C++"
REM        workload (MSVC + Windows SDK).
REM
REM NOTE: keep this file ASCII-only, otherwise cmd.exe (GBK console) may
REM       mis-parse it.
REM ---------------------------------------------------------------------------
setlocal enabledelayedexpansion

set "VSDIR="
for %%P in (
  "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
  "C:\Program Files (x86)\Microsoft Visual Studio\2022\Community"
  "C:\Program Files\Microsoft Visual Studio\2022\Community"
  "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community"
  "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools"
) do (
  if exist "%%~P\VC\Auxiliary\Build\vcvars64.bat" if not defined VSDIR set "VSDIR=%%~P"
)

if not defined VSDIR (
  echo [x] vcvars64.bat not found. Install "Desktop development with C++" in Visual Studio.
  exit /b 1
)

call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

set "CMAKE=cmake"
where cmake >nul 2>nul || set "CMAKE=%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

set "ROOT=%~dp0.."
echo [*] configure (Ninja, Release) ...
"%CMAKE%" -S "%ROOT%\windows_preview" -B "%ROOT%\build" -G Ninja -DCMAKE_BUILD_TYPE=Release || exit /b 1
echo [*] build ...
"%CMAKE%" --build "%ROOT%\build" || exit /b 1
echo [ok] %ROOT%\build\pc_gl_viewer_preview.exe
endlocal
