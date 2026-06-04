@echo off
setlocal

set "VSINSTALLDIR=C:\Program Files\Microsoft Visual Studio\18\Community"
set "MSVC_VER=14.51.36231"
set "MSVC_PATH=%VSINSTALLDIR%\VC\Tools\MSVC\%MSVC_VER%"
set "SDK_VER=10.0.26100.0"
set "SDK_PATH=C:\Program Files (x86)\Windows Kits\10"

set "PATH=%MSVC_PATH%\bin\Hostx64\x64;%SDK_PATH%\bin\%SDK_VER%\x64;%PATH%"
set "INCLUDE=%MSVC_PATH%\include;%SDK_PATH%\Include\%SDK_VER%\ucrt;%SDK_PATH%\Include\%SDK_VER%\um;%SDK_PATH%\Include\%SDK_VER%\shared"
set "LIB=%MSVC_PATH%\lib\x64;%SDK_PATH%\Lib\%SDK_VER%\ucrt\x64;%SDK_PATH%\Lib\%SDK_VER%\um\x64"

set "CFLAGS=/c /EHsc /std:c++17 /utf-8 /W3 /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /DNOMINMAX /I src /I src/imgui"
set "IMGUI_CFLAGS=/c /EHsc /std:c++17 /utf-8 /W3 /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /DNOMINMAX /I src/imgui"
set "LFLAGS=advapi32.lib shell32.lib gdiplus.lib comctl32.lib user32.lib gdi32.lib psapi.lib d3d11.lib dxgi.lib d3dcompiler.lib"
set "LINK_OPTS=/link /SUBSYSTEM:WINDOWS"

echo === Compiling Hook DLL ===
cl /c /EHsc /std:c++17 /utf-8 /D_CRT_SECURE_NO_WARNINGS /DNOMINMAX dll\steam_hook.cpp /Fo:bin\steam_hook.obj
if %ERRORLEVEL% neq 0 goto :error
link /DLL /OUT:bin\steam_hook.dll bin\steam_hook.obj kernel32.lib user32.lib
if %ERRORLEVEL% neq 0 goto :error

cd /d "E:\test\deepseek_temp\steam_launcher"

if not exist bin mkdir bin

echo === Compiling ImGui core ===
cl %IMGUI_CFLAGS% src\imgui\imgui.cpp        /Fo:bin\imgui.obj
if %ERRORLEVEL% neq 0 goto :error
cl %IMGUI_CFLAGS% src\imgui\imgui_demo.cpp    /Fo:bin\imgui_demo.obj
if %ERRORLEVEL% neq 0 goto :error
cl %IMGUI_CFLAGS% src\imgui\imgui_draw.cpp    /Fo:bin\imgui_draw.obj
if %ERRORLEVEL% neq 0 goto :error
cl %IMGUI_CFLAGS% src\imgui\imgui_tables.cpp  /Fo:bin\imgui_tables.obj
if %ERRORLEVEL% neq 0 goto :error
cl %IMGUI_CFLAGS% src\imgui\imgui_widgets.cpp /Fo:bin\imgui_widgets.obj
if %ERRORLEVEL% neq 0 goto :error
cl %IMGUI_CFLAGS% src\imgui\imgui_impl_win32.cpp  /Fo:bin\imgui_impl_win32.obj
if %ERRORLEVEL% neq 0 goto :error
cl %IMGUI_CFLAGS% src\imgui\imgui_impl_dx11.cpp   /Fo:bin\imgui_impl_dx11.obj
if %ERRORLEVEL% neq 0 goto :error

echo === Compiling vdf_parser.cpp ===
cl %CFLAGS% src\vdf_parser.cpp /Fo:bin\vdf_parser.obj

echo === Compiling binary_vdf.cpp ===
cl %CFLAGS% src\binary_vdf.cpp /Fo:bin\binary_vdf.obj
if %ERRORLEVEL% neq 0 goto :error

echo === Compiling steam_scanner.cpp ===
cl %CFLAGS% src\steam_scanner.cpp /Fo:bin\steam_scanner.obj
if %ERRORLEVEL% neq 0 goto :error

echo === Compiling game_launcher.cpp ===
cl %CFLAGS% src\game_launcher.cpp /Fo:bin\game_launcher.obj
if %ERRORLEVEL% neq 0 goto :error

echo === Compiling category_manager.cpp ===
cl %CFLAGS% src\category_manager.cpp /Fo:bin\category_manager.obj
if %ERRORLEVEL% neq 0 goto :error

echo === Compiling playtime_tracker.cpp ===
cl %CFLAGS% src\playtime_tracker.cpp /Fo:bin\playtime_tracker.obj
if %ERRORLEVEL% neq 0 goto :error

echo === Compiling steam_watcher.cpp ===
cl %CFLAGS% src\steam_watcher.cpp /Fo:bin\steam_watcher.obj
if %ERRORLEVEL% neq 0 goto :error

echo === Compiling dll_injector.cpp ===
cl %CFLAGS% src\dll_injector.cpp /Fo:bin\dll_injector.obj
if %ERRORLEVEL% neq 0 goto :error

echo === Compiling icon_loader.cpp ===
cl %CFLAGS% src\icon_loader.cpp /Fo:bin\icon_loader.obj
if %ERRORLEVEL% neq 0 goto :error

echo === Compiling gui_app.cpp ===
cl %CFLAGS% src\gui_app.cpp /Fo:bin\gui_app.obj
if %ERRORLEVEL% neq 0 goto :error

echo === Compiling win_main.cpp ===
cl %CFLAGS% src\win_main.cpp /Fo:bin\win_main.obj
if %ERRORLEVEL% neq 0 goto :error

echo === Linking (GUI) ===
cl bin\imgui.obj bin\imgui_demo.obj bin\imgui_draw.obj bin\imgui_tables.obj bin\imgui_widgets.obj bin\imgui_impl_win32.obj bin\imgui_impl_dx11.obj bin\vdf_parser.obj bin\binary_vdf.obj bin\steam_scanner.obj bin\game_launcher.obj bin\category_manager.obj bin\playtime_tracker.obj bin\steam_watcher.obj bin\dll_injector.obj bin\icon_loader.obj bin\gui_app.obj bin\win_main.obj /Fe:bin\SteamLauncher.exe %LFLAGS% %LINK_OPTS%
if %ERRORLEVEL% neq 0 goto :error

echo.
echo ==========================================
echo  Build SUCCESS: bin\SteamLauncher.exe
echo ==========================================
goto :end

:error
echo.
echo ==========================================
echo  Build FAILED!
echo ==========================================
:end
endlocal
