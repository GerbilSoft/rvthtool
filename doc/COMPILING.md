# Compiling rvthtool

## Linux

On Debian/Ubuntu, you will need build-essential and the following development
packages:
* pkg-config libgmp-dev nettle-dev libudev-dev

On Red Hat/Fedora, you will need to install "C Development Tools and Libraries"
and the following development packages:
* cmake gmp-devel nettle-devel libudev-devel

Clone the repository, then:
* cd rvthtool
* mkdir build
* cd build
* cmake .. -DCMAKE_INSTALL_PREFIX=/usr
* make
* sudo make install

### Building .deb Packages

You will need to install the following:
* devscripts
* debhelper

In order to build debug symbol packages, you will need:
* Debian: debhelper >= 9.20151219
* Ubuntu: pkg-create-dbgsym

In the top-level source directory, run this command:
* `debuild -i -us -uc -b`

Assuming everything builds correctly, the .deb packages should be built in
the directory above the top-level source directory.

## Windows

The Windows version requires one of the following compilers: (minimum versions)
* Microsoft Visual C++ 2010 with the Windows 7 SDK
* gcc-4.5 with MinGW-w64

You will also need to install [CMake](https://cmake.org/download/), since the
project uses the CMake build system.

Clone the repository, then open an MSVC or MinGW command prompt and run the
following commands from your rom-properties repository directory:
* mkdir build
* cd build
* cmake .. -G "Visual Studio 15 2017"
* make

Replace "Visual Studio 15 2017" with the version of Visual Studio you have
installed. Add "Win64" after the year for a 64-bit version.

See README.md for general usage information.
