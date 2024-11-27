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

# == SDL CONFIG ==
SDL2_FLAGS = $(shell sdl2-config --cflags --libs)
SDL2_FLAGS_STATIC = $(shell sdl2-config --cflags --static-libs)

# == DETECT PLATFORM ==
# WIN = Windows 10, MAC = MacOS, RPI = Raspberry Pi 4
ifeq ($(OS), Windows_NT)
	# == WINDOWS ==
	# platform id (for defines, different targets, etc)
	PLATFORM=WIN
	# windows has a standard extension for executables (the other platforms don't)
	APP_EXTENSION=.exe
	# windows doesn't have a standard location to install libraries but I've chosen to use C:
	SDL2_ROOT=C:\\SDL2
	# on windows I'm using the DLL version of SDL2
	SDL2_LIB_SRC=SDL2.dll
	SDL2_LIB=SDL2.dll
	# path to the SDL2 library
	SDL2_PATH=${SDL2_ROOT}\\
	# windows also requires specifying the location of the library headers
	SDL2_INCLUDE_PATH=${SDL2_ROOT}\\include\\
	DEBUG_FLAGS=${SDL2_FLAGS} -mconsole
	RELEASE_FLAGS=${SDL2_FLAGS_STATIC}
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
		DEBUG_FLAGS=${SDL2_FLAGS}
		RELEASE_FLAGS=${SDL2_FLAGS_STATIC}
		# mac app bundles require the app binary to be in a specific sub-directory
		MAC_APP_BUNDLE_ROOT=${APP_NAME}.app/Contents
		BUILD_RELEASE_BINARY_SUBDIR=${MAC_APP_BUNDLE_ROOT}/MacOS
		# games also need to be in the bundle for the app to have permission to open them
		BUILD_RELEASE_GAMES_SUBDIR=${MAC_APP_BUNDLE_ROOT}/Resources/games
	else
		ifeq ($(shell uname -s), Linux)
			ifeq ($(shell uname -m), x86_64)
				# == LINUX ==
				PLATFORM=LIN
				# path to the SDL2 library file
				SDL2_PATH=/usr/lib/x86_64-linux-gnu/
				# SDL2 library file
				SDL2_LIB_SRC=libSDL2-2.0.so.0.18.2
				SDL2_LIB=libSDL2-2.0.so.0
			else
				# == RASPBERRY PI ==
				PLATFORM=RPI
				# path to the SDL2 library file
				SDL2_PATH=/lib/arm-linux-gnueabihf/
				# SDL2 library file
				SDL2_LIB_SRC=libSDL2-2.0.so.0.9.0
				SDL2_LIB=libSDL2-2.0.so.0
			endif

			DEBUG_FLAGS=${SDL2_FLAGS} -lm
			RELEASE_FLAGS=${SDL2_FLAGS_STATIC}
			# for linux, also build a dynamically-linked version for release, so users can supply their own SDL2 installation
			RELEASE_FLAGS_DYNAMIC=${SDL2_FLAGS} -lm
			BUILD_RELEASE_GAMES_SUBDIR=games
		endif
	endif
endif

# == BUILD VARIABLES ==
# pound-define to allow for platform differences in C code
PLATFORM_DEFINE=PLATFORM_${PLATFORM}
# the app binary filename is the app's name + the platform extension (if any)
APP_BINARY=${APP_NAME}${APP_EXTENSION}
# for linux, we also build a dynamically linked version of the binary
APP_BINARY_DYNAMIC=${APP_NAME}_dyn${APP_EXTENSION}
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
release: clean-release embed-js build-release-${PLATFORM} package-release-${PLATFORM}

embed-js:
	${JS} util/embed.js ./src/bitsy/engine ./src/bitsybox
	${JS} util/embed.js ./src/bitsy/font ./src/bitsybox
	${JS} util/embed.js ./src/boot ./src/bitsybox
	${JS} util/embed.js ./src/tune ./src/bitsybox

build-release:
	${MAKE_DIRECTORY} ${BIN_DIR}
	$(CC) $(SRC_FILES) ${RELEASE_FLAGS} -D${PLATFORM_DEFINE} -o ${BIN_DIR}/$(APP_BINARY)

build-release-WIN: build-release

build-release-MAC: build-release

build-release-LIN: build-release
	$(CC) $(SRC_FILES) ${RELEASE_FLAGS_DYNAMIC} -D${PLATFORM_DEFINE} -o ${BIN_DIR}/$(APP_BINARY_DYNAMIC)

build-release-RPI: build-release
	$(CC) $(SRC_FILES) ${RELEASE_FLAGS_DYNAMIC} -D${PLATFORM_DEFINE} -o ${BIN_DIR}/$(APP_BINARY_DYNAMIC)

package-release:
	${MAKE_DIRECTORY} ${BUILD_RELEASE_BINARY_DIR}
	${MAKE_DIRECTORY} ${BUILD_RELEASE_GAMES_DIR}
	${COPY_FILES} ${BIN_DIR}/$(APP_BINARY) ${BUILD_RELEASE_BINARY_DIR}/${APP_BINARY}
	${COPY_FILES} res/demo_games/* ${BUILD_RELEASE_GAMES_DIR}
	${COPY_FILES} doc/MANUAL.txt ${BUILD_RELEASE_DIR}/MANUAL.txt
	${COPY_FILES} res/LICENSE.txt ${BUILD_RELEASE_DIR}/LICENSE.txt

package-release-WIN: package-release

package-release-MAC: package-release
	${COPY_FILES} res/mac_app_bundle/Info.plist ${BUILD_RELEASE_DIR}/${MAC_APP_BUNDLE_ROOT}/Info.plist
	${MAKE_ALIAS} ${BUILD_RELEASE_GAMES_SUBDIR} ${BUILD_RELEASE_DIR}/games

package-release-LIN: package-release
	${COPY_FILES} ${BIN_DIR}/$(APP_BINARY_DYNAMIC) ${BUILD_RELEASE_BINARY_DIR}/${APP_BINARY_DYNAMIC}

package-release-RPI: package-release
	${COPY_FILES} ${BIN_DIR}/$(APP_BINARY_DYNAMIC) ${BUILD_RELEASE_BINARY_DIR}/${APP_BINARY_DYNAMIC}

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
	${COPY_FILES} src/test ${BUILD_DEBUG_DIR}/test
	${COPY_FILES} src/tune ${BUILD_DEBUG_DIR}/tune
	${COPY_FILES} res/demo_games ${BUILD_DEBUG_DIR}/games

package-debug-WIN: package-debug
	${COPY_FILES} ${SDL2_PATH}${SDL2_LIB_SRC} ${BUILD_DEBUG_DIR}/${SDL2_LIB}

package-debug-MAC: package-debug

package-debug-LIN: package-debug

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