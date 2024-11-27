# bitsybox

~ a [bitsy](https://bitsy.org) emulator ~

features:

- play `.bitsy` game files
- keyboard and gamepad controls supported
- windowed and fullscreen modes
- platforms: windows, macOS, raspberry pi 4, linux

# build instructions

these are instructions for building bitsybox from source - you can also download [pre-built versions from itch.io](https://ledoux.itch.io/bitsybox)

## requirements

to build bitsybox, you'll need the following:

- a bash-compatible shell that can run the sdl2-config script and standard bash commands used by the bitsybox makefile
- make: for running the makefile
- gcc: C compiler for compiling the program
- [SDL2](https://wiki.libsdl.org/SDL2/Installation): cross platform framework for graphics, audio, input, etc.
- [Node.js](https://nodejs.org/): used to run some utility scripts as part of the build process
- bitsybox source code

## linux

on linux you should already have make, bash, and gcc available

- install SDL2 from your package manager
	- the command may vary based on your distribution, but should be something like `apt install libsdl2-dev`
- install Node.js from your package manager
	- again the command may vary, but will look something like `apt install nodejs`
- from the command line, navigate to the root directory of the bitsybox source code
- run `make`

## mac

on mac you should already have a bash-like command line shell

- for make and gcc, install the [xcode command line tools](https://developer.apple.com/xcode/resources/)
- install SDL2 (either follow instructions on its website or use a package manager such as [homebrew](https://brew.sh/))
	- if using homebrew, the command is `brew install sdl2`
- install Node.js (again either from its website or using homebrew or another package manager)
	- if using homebrew, the command is `brew install node`
- from the command line, navigate to the root directory of the bitsybox source code
- run `make`

## windows

on windows you will need to install a bash-compatible shell - i suggest using MSYS2, which also comes with a package manager you can use to install the other dependencies

if using MSYS2:

- install [MSYS2](https://www.msys2.org/)
- using MSYS2, install make, gcc, SDL2, and Node.js
- from the MSYS2 terminal, navigate to the root directory of the bitsybox source code
- run `make`