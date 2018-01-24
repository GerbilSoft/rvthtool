@ECHO OFF
cmake --version
IF "%PLATFORM%" == "x64" (
	cmake . -G "Visual Studio 12 2013 Win64" -DCMAKE_GENERATOR_TOOLSET=v120_xp -DBUILD_TESTING=ON
) ELSE (
	cmake . -G "Visual Studio 12 2013" -DCMAKE_GENERATOR_TOOLSET=v120_xp -DBUILD_TESTING=ON
)
