@echo off
REM ---------------------------------------------------------------------------
REM 无 ROS 环境下的「编译期校验」：用 tools\ros2_stub_check\stubs 下的最小桩头文件
REM （rclcpp / sensor_msgs / GLFW）编译 ROS 节点与 GLFW 平台层，抓语法与类型错。
REM
REM 注意：这只证明代码能编译，不能证明 ROS API 语义正确；
REM       最终验证必须在 Ubuntu + ROS2 上用 colcon build 完成。
REM
REM 用法: scripts\check_ros2_stubs.cmd
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
set "OUT=%ROOT%\build_stubcheck"

if not exist "%OUT%" mkdir "%OUT%"

echo [*] compile ROS2 nodes with stub headers ...
cl /nologo /c /EHsc /std:c++17 /utf-8 /permissive- /W3 /wd4819 /MD /DNOMINMAX ^
  /D_CRT_SECURE_NO_WARNINGS /Fo"%OUT%\\" ^
  /I"%PKG%\tools\ros2_stub_check\stubs" /I"%PKG%\include" /I"%ROOT%\windows_preview\include" ^
  "%PKG%\src\ros\pc_cloud_publisher_main.cpp" ^
  "%PKG%\src\ros\pc_gl_viewer_main.cpp" ^
  "%PKG%\src\ros\cloud_publisher_node.cpp" ^
  "%PKG%\src\ros\gl_viewer_node.cpp" ^
  "%PKG%\src\platform\glfw_window.cpp"
if errorlevel 1 (
  echo [x] stub check FAILED
  exit /b 1
)
echo [ok] stub check passed: ROS2 nodes + GLFW backend compile cleanly
endlocal
