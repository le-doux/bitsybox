# == APP INFO ==
APP_NAME=bitsybox

# == LANGUAGE ENVIRONMENTS ==
# c compiler
CC=gcc
# javascript interpreter (for utility scripts)
JS=node

# == CONSOLE COMMANDS ==
# copy files recursively
COPY_FILES=cp -R
# make directory (if it doesn't exist)
MAKE_DIRECTORY=mkdir -p
# remove directory recursively (if it exists)
REMOVE_DIRECTORY=rm -rf
# make alias to directory (shortcut)
MAKE_ALIAS=ln -s

# == DETECT PLATFORM ==
# WIN = Windows 10, MAC = MacOS, RPI = Raspberry Pi 4
ifeq ($(OS), Windows_NT)
	# == WINDOWS ==
	# platform id (for defines, different targets, etc)
	PLATFORM=WIN
	# windows has a standard extension for executables (the other platforms don't)
	APP_EXTENSION=.exe
	# windows doesn't have a standard location to install libraries but I've chosen to use C:
	SDL2_ROOT=C:\\SDL2-2.0.16
	# on windows I'm using the DLL version of SDL2
	SDL2_LIB_SRC=SDL2.dll
	SDL2_LIB=SDL2.dll
	# path to the SDL2 library
	SDL2_PATH=${SDL2_ROOT}\\lib\\x86\\
	# windows also requires specifying the location of the library headers
	SDL2_INCLUDE_PATH=${SDL2_ROOT}\\include\\
	DEBUG_FLAGS=-I${SDL2_INCLUDE_PATH} -L${SDL2_PATH} -lSDL2main -lSDL2
	# add the "-mwindows" flag in release builds to hide console output
	RELEASE_FLAGS=${DEBUG_FLAGS} -mwindows
	BUILD_RELEASE_GAMES_SUBDIR=games
else
	ifeq ($(shell uname -s), Darwin)
		# == MAC ==
		# platform id
		PLATFORM=MAC
		# on mac I'm using the framework version of SDL2
		SDL2_LIB_SRC=SDL2.framework
		SDL2_LIB=SDL2.framework
		# look for SDL2 from the standard install location for frameworks on macos
		SDL2_PATH=/Library/Frameworks/
		DEBUG_FLAGS=-framework SDL2
		# add an -rpath so the release version can find SDL2.framework in the app bundle
		RELEASE_FLAGS=${DEBUG_FLAGS} -Wl,-rpath,@executable_path/../Frameworks
		# mac app bundles require the app binary to be in a specific sub-directory
		MAC_APP_BUNDLE_ROOT=${APP_NAME}.app/Contents
		BUILD_RELEASE_BINARY_SUBDIR=${MAC_APP_BUNDLE_ROOT}/MacOS
		# put SDL2 framework in a consistent spot in the app bundle
		BUILD_RELEASE_LIBRARY_SUBDIR=${MAC_APP_BUNDLE_ROOT}/Frameworks
		# games also need to be in the bundle for the app to have permission to open them
		BUILD_RELEASE_GAMES_SUBDIR=${MAC_APP_BUNDLE_ROOT}/Resources/games
	else
		# == RASPBERRY PI ==
		# NOTE: I don't know how to detect this system so I'm defaulting to it if I don't detect MAC or WIN
		# platform id
		PLATFORM=RPI
		# SDL2 library file
		SDL2_LIB_SRC=libSDL2-2.0.so.0.9.0
		SDL2_LIB=libSDL2-2.0.so.0
		# path where to the SDL2 library file
		SDL2_PATH=/lib/arm-linux-gnueabihf/
		DEBUG_FLAGS=-lSDL2 -lm
		# add an -rpath so I can bundle the .so file with the app binary so users don't need to install SDL2
		RELEASE_FLAGS=${DEBUG_FLAGS} -Wl,-rpath,'$$ORIGIN'
		BUILD_RELEASE_GAMES_SUBDIR=games
	endif
endif

# == BUILD VARIABLES ==
# pound-define to allow for platform differences in C code
PLATFORM_DEFINE=PLATFORM_${PLATFORM}
# the app binary filename is the app's name + the platform extension (if any)
APP_BINARY=${APP_NAME}${APP_EXTENSION}
# path to C source files
SRC_FILES=src/bitsybox/*.c src/bitsybox/duktape/*.c
# build directories
BIN_DIR=build/bin
BUILD_DEBUG_DIR=$(BUILD_RELEASE_DIR)_DEBUG
BUILD_RELEASE_DIR=build/$(APP_NAME)_$(PLATFORM)
BUILD_RELEASE_BINARY_DIR=${BUILD_RELEASE_DIR}/${BUILD_RELEASE_BINARY_SUBDIR}
BUILD_RELEASE_LIBRARY_DIR=${BUILD_RELEASE_DIR}/${BUILD_RELEASE_LIBRARY_SUBDIR}
BUILD_RELEASE_GAMES_DIR=${BUILD_RELEASE_DIR}/${BUILD_RELEASE_GAMES_SUBDIR}

# == RELEASE TARGET ==
release: clean-release embed-js build-release package-release-${PLATFORM}

embed-js:
	${JS} util/embed src/bitsy/engine src/bitsybox
	${JS} util/embed src/bitsy/font src/bitsybox
	${JS} util/embed src/boot src/bitsybox

build-release:
	${MAKE_DIRECTORY} ${BIN_DIR}
	$(CC) $(SRC_FILES) ${RELEASE_FLAGS} -D${PLATFORM_DEFINE} -o ${BIN_DIR}/$(APP_BINARY)

package-release:
	${MAKE_DIRECTORY} ${BUILD_RELEASE_BINARY_DIR}
	${MAKE_DIRECTORY} ${BUILD_RELEASE_LIBRARY_DIR}
	${MAKE_DIRECTORY} ${BUILD_RELEASE_GAMES_DIR}
	${COPY_FILES} ${SDL2_PATH}${SDL2_LIB_SRC} ${BUILD_RELEASE_LIBRARY_DIR}/${SDL2_LIB}
	${COPY_FILES} ${BIN_DIR}/$(APP_BINARY) ${BUILD_RELEASE_BINARY_DIR}/${APP_BINARY}
	${COPY_FILES} res/demo_games/* ${BUILD_RELEASE_GAMES_DIR}
	${COPY_FILES} doc/MANUAL.txt ${BUILD_RELEASE_DIR}/MANUAL.txt
	${COPY_FILES} res/LICENSE.txt ${BUILD_RELEASE_DIR}/LICENSE.txt

package-release-WIN: package-release

package-release-MAC: package-release
	${COPY_FILES} res/mac_app_bundle/Info.plist ${BUILD_RELEASE_DIR}/${MAC_APP_BUNDLE_ROOT}/Info.plist
	${MAKE_ALIAS} ${BUILD_RELEASE_GAMES_SUBDIR} ${BUILD_RELEASE_DIR}/games

package-release-RPI: package-release

# == DEBUG TARGET ==
debug: clean-debug build-debug package-debug-${PLATFORM}

build-debug:
	${MAKE_DIRECTORY} ${BIN_DIR}
	$(CC) $(SRC_FILES) $(DEBUG_FLAGS) -D${PLATFORM_DEFINE} -DBUILD_DEBUG -o ${BIN_DIR}/$(APP_BINARY)

package-debug:
	${MAKE_DIRECTORY} ${BUILD_DEBUG_DIR}
	${COPY_FILES} ${BIN_DIR}/$(APP_BINARY) ${BUILD_DEBUG_DIR}/$(APP_BINARY)
	${COPY_FILES} src/bitsy ${BUILD_DEBUG_DIR}/bitsy
	${COPY_FILES} src/boot ${BUILD_DEBUG_DIR}/boot
	${COPY_FILES} res/demo_games ${BUILD_DEBUG_DIR}/games

package-debug-WIN: package-debug
	${COPY_FILES} ${SDL2_PATH}${SDL2_LIB_SRC} ${BUILD_DEBUG_DIR}/${SDL2_LIB}

package-debug-MAC: package-debug

package-debug-RPI: package-debug

# == CLEAN TARGETS ==
clean-release:
	${REMOVE_DIRECTORY} $(BIN_DIR)
	${REMOVE_DIRECTORY} $(BUILD_RELEASE_DIR)

clean-debug:
	${REMOVE_DIRECTORY} $(BIN_DIR)
	${REMOVE_DIRECTORY} $(BUILD_DEBUG_DIR)

clean:
	${REMOVE_DIRECTORY} build
