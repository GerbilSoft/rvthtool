@ECHO OFF
cmake --version

:: FIXME: Restore MinGW-w64 support?
set compiler=msvc2015

if "%compiler%" == "msvc2015" goto :msvc2015
if "%compiler%" == "mingw-w64" goto :mingw-w64
echo *** ERROR: Unsupported compiler '%compiler%'.
exit /b 1

:msvc2015
set PreferredToolArchitecture=x64
set "CMAKE_GENERATOR=Visual Studio 14 2015"
set CMAKE_GENERATOR_TOOLSET=v140_xp
if "%platform%" == "x64" set "CMAKE_GENERATOR=%CMAKE_GENERATOR% Win64"
if "%platform%" == "x86" set Qt5_DIR=C:\Qt\5.13\msvc2015\lib\cmake\Qt5
if "%platform%" == "x64" set Qt5_DIR=C:\Qt\5.13\msvc2015_64\lib\cmake\Qt5
mkdir build
cd build
cmake .. -G "%CMAKE_GENERATOR%" -DCMAKE_GENERATOR_TOOLSET=%CMAKE_GENERATOR_TOOLSET% -DBUILD_TESTING=ON -DQt5_DIR=%Qt5_DIR%
exit /b %ERRORLEVEL%

:mingw-w64
set PATH=%PATH:C:\Program Files\Git\bin;=%
set PATH=%PATH:C:\Program Files\Git\usr\bin;=%
set PATH=%PATH:C:\Program Files (x86)\Git\bin;=%
set PATH=%PATH:C:\Program Files (x86)\Git\usr\bin;=%
if "%platform%" == "x86" set MINGW64_ROOT=C:/mingw-w64/i686-6.3.0-posix-dwarf-rt_v5-rev1/mingw32
if "%platform%" == "x64" set MINGW64_ROOT=C:/mingw-w64/x86_64-7.2.0-posix-seh-rt_v5-rev1/mingw64
if "%playform%" == "x86" set Qt5_DIR=C:\Qt\5.8\mingw53_32\lib\cmake\Qt5
if "%playform%" == "x64" set Qt5_DIR=C:\Qt\5.8\mingw53_64\lib\cmake\Qt5
set "PATH=%MINGW64_ROOT%\bin;%PATH%"

:: FIXME: gtest is failing on AppVeyor because "AutoHandle" does not name a type.
mkdir build
cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=%MINGW64_ROOT% -DCMAKE_C_COMPILER=%MINGW64_ROOT%/bin/gcc.exe -DCMAKE_CXX_COMPILER=%MINGW64_ROOT%/bin/g++.exe -DCMAKE_BUILD_TYPE=%configuration% -DBUILD_TESTING=OFF -DQt5_DIR=%Qt5_DIR%
exit /b %ERRORLEVEL%
