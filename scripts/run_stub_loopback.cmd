@echo off
REM ---------------------------------------------------------------------------
REM 无 ROS 环境下的「数据链路回环测试」：
REM   Windows + MSVC + 桩头文件(rclcpp/sensor_msgs) 下，把真实节点代码跑起来：
REM   发布节点填 PointCloud2 -> 桩传输 -> 适配器 -> 解码 -> 订阅节点回调。
REM
REM 用法: scripts\run_stub_loopback.cmd
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
  echo [x] vcvars64.bat not found.
  exit /b 1
)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

set "ROOT=%~dp0.."
set "PKG=%ROOT%\ros2_ws\src\pc_gl_viewer"
set "OUT=%ROOT%\build_loopback"
if not exist "%OUT%" mkdir "%OUT%"

echo [*] compile loopback test (stub ROS transport) ...
cl /nologo /EHsc /std:c++17 /utf-8 /permissive- /W3 /wd4819 /MD /DNOMINMAX ^
  /D_CRT_SECURE_NO_WARNINGS /Fo"%OUT%\\" /Fe"%OUT%\ros_loopback_test.exe" ^
  /I"%PKG%\tools\ros2_stub_check\stubs" /I"%PKG%\include" ^
  "%PKG%\tools\ros2_stub_check\loopback_test.cpp" ^
  "%PKG%\src\ros\cloud_publisher_node.cpp" ^
  "%PKG%\src\ros\gl_viewer_node.cpp" ^
  "%PKG%\src\core\cloud_wire.cpp" ^
  "%PKG%\src\core\point_cloud.cpp" ^
  "%PKG%\src\core\synth_scene.cpp" ^
  "%PKG%\src\core\camera.cpp" ^
  "%PKG%\src\core\gl_api.cpp" ^
  "%PKG%\src\core\screenshot.cpp" ^
  "%PKG%\src\core\renderer.cpp" ^
  "%PKG%\src\core\hud.cpp" ^
  "%PKG%\src\core\app.cpp" ^
  /link opengl32.lib gdi32.lib user32.lib
if errorlevel 1 (
  echo [x] build FAILED
  exit /b 1
)

echo.
"%OUT%\ros_loopback_test.exe"
set "RC=%ERRORLEVEL%"
if not "%RC%"=="0" exit /b %RC%
echo [ok] loopback test passed
endlocal
