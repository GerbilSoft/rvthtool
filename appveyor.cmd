@ECHO OFF
cmake --version
IF "%PLATFORM%" == "x64" (
	cmake . -G "Visual Studio 14 2015 Win64" -DCMAKE_GENERATOR_TOOLSET=v140_xp -DBUILD_TESTING=ON
) ELSE (
	cmake . -G "Visual Studio 14 2015" -DCMAKE_GENERATOR_TOOLSET=v140_xp -DBUILD_TESTING=ON
)
