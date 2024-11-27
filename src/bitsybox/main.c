#include <stdio.h>
#include <math.h>
#include <string.h>
#include <unistd.h> // for getcwd, chdir
#include <limits.h> // for PATH_MAX
#include <dirent.h>
#include "duktape/duktape.h"
#include "SDL.h"

/* # DEBUG-ONLY INCLUDES */

#ifndef BUILD_DEBUG
#include "engine.h"
#include "font.h"
#include "boot.h"
#include "tune.h"
#endif

/* # TEST SETTINGS */

// #define DEMO_MODE
// #define TUNE_TOOL_MODE
#define ENABLE_BITSY_LOG

/* # GLOBALS */

int shouldContinue = 1;

/* # SDL */

SDL_Window* window;
SDL_Event event;
SDL_Renderer* renderer;

/* # DUKTAPE */

static void fatalError(void* udata, const char* msg) {
	(void) udata; // not used currently
	fprintf(stderr, "*** FATAL ERROR: %s\n", (msg ? msg : "no message"));
	fflush(stderr);
	abort();
}

/* # WINDOW */

#if defined(PLATFORM_RPI) && !defined(BUILD_DEBUG)
int isFullscreen = 1;
#else
int isFullscreen = 0;
#endif
int isFullscreenShortcut = 0;
int didWindowResizeThisFrame = 0;

int windowWidth = 0;
int windowHeight = 0;

void onWindowResize() {
	SDL_GetWindowSize(window, &windowWidth, &windowHeight);
}

/* # MEMORY */

typedef struct MemoryBlock {
	uint32_t size;
	uint8_t* data;
} MemoryBlock;

#define MEMORY_BLOCK_MAX 1024
MemoryBlock memory[MEMORY_BLOCK_MAX];

void freeMemoryBlock(int block) {
	if (memory[block].data != NULL) {
		free(memory[block].data);
	}

	memory[block].size = -1;
	memory[block].data = NULL;
}

void initializeMemoryBlocks() {
	for (int i = 0; i < MEMORY_BLOCK_MAX; i++) {
		freeMemoryBlock(i);
	}
}

void allocateMemoryBlock(int block, uint16_t size) {
	// free any existing memory before re-allocating it!
	freeMemoryBlock(block);

	memory[block].size = size;
	memory[block].data = calloc(size, sizeof(uint8_t));
}

int isMemoryBlockEmpty(int block) {
	return memory[block].size <= 0 || memory[block].data == NULL;
}

int isMemoryBlockValid(int block) {
	return block >= 0 && block < MEMORY_BLOCK_MAX && !isMemoryBlockEmpty(block);
}

/* # GRAPHICS */

typedef struct Color {
	int r;
	int g;
	int b;
} Color;

#define PALETTE_MAX 256
Color systemPalette[PALETTE_MAX];

#define TEXTURE_MAX MEMORY_BLOCK_MAX
SDL_Texture* textures[TEXTURE_MAX];

// graphics modes
int curGraphicsMode;
int curTextMode;

// render scales
int renderScale = 4;
int textboxRenderScale = 2;

// should SDL textures be re-rendered this frame?
int shouldRenderTextures = 0;

// textbox state
int isTextboxVisible = 0;
int textboxX = 0;
int textboxY = 0;
int textboxWidth = 0;
int textboxHeight = 0;

/* # INPUT */

// keyboard
int isButtonUp = 0;
int isButtonDown = 0;
int isButtonLeft = 0;
int isButtonRight = 0;
int isButtonW = 0;
int isButtonA = 0;
int isButtonS = 0;
int isButtonD = 0;
int isButtonR = 0;
int isButtonSpace = 0;
int isButtonReturn = 0;
int isButtonEscape = 0;
int isButtonLCtrl = 0;
int isButtonRCtrl = 0;
int isButtonLAlt = 0;
int isButtonRAlt = 0;

// gamepad
int isButtonPadUp = 0;
int isButtonPadDown = 0;
int isButtonPadLeft = 0;
int isButtonPadRight = 0;
int isButtonPadA = 0;
int isButtonPadB = 0;
int isButtonPadX = 0;
int isButtonPadY = 0;
int isButtonPadStart = 0;

// mouse
int mouseX = 0;
int mouseY = 0;
int isLeftMouseDown = 0;

/* # AUDIO */

// samples per second
#define AUDIO_SAMPLE_RATE 44100
// size of the audio buffer in samples (filled by audio callback)
#define AUDIO_BUFFER_SIZE 256

int audioStep = 0; // global audio step counter for sampling
float audioVolume = 0.0f; // global audio volume (range: 0.0 - 1.0)

typedef struct PulseWave {
	int cycle; // cycle length in sample steps
	int duty; // duty length in sample steps
} PulseWave;

PulseWave wave(float frequency, float dutyCycle) {
	// calculate cycle length in samples
	float cycle = AUDIO_SAMPLE_RATE / frequency;
	// calcualte duty lenght in samples
	float duty = cycle * dutyCycle;

	// convert cycle and duty to integer steps
	return (PulseWave) {
		.cycle = floor(cycle),
		.duty = floor(duty),
	};
}

int pulse(PulseWave* wave, int step) {
	return (step % wave->cycle) <= wave->duty ? 1 : 0;	
}

// sound channels
PulseWave soundChannel1;
float volumeChannel1 = 0.0f; // volume from 0.0 - 1.0
int durationChannel1 = 0; // duration in *samples* (not frames or ms)
int dutyChannel1;

PulseWave soundChannel2;
float volumeChannel2 = 0.0f; // volume from 0.0 - 1.0
int durationChannel2 = 0; // duration in *samples* (not frames or ms)
int dutyChannel2;

void audioCallback(void* userdata, Uint8* stream, int len) {
	float* fstream = (float*) stream;

	for (int i = 0; i < AUDIO_BUFFER_SIZE; i++) {
		// increment global audio step
		audioStep++;

		// decrement channel duration counters and mute audio when they reach zero
		durationChannel1--;
		if (durationChannel1 <= 0) {
			volumeChannel1 = 0.0f;
			durationChannel1 = 0;
		}

		durationChannel2--;
		if (durationChannel2 <= 0) {
			volumeChannel2 = 0.0f;
			durationChannel2 = 0;
		}

		// calculate pulse wave sample for channel 1
		fstream[(i * 2) + 0] = pulse(&soundChannel1, audioStep) * volumeChannel1 * audioVolume;

		// calculate pulse wave sample for channel 2
		fstream[(i * 2) + 1] = pulse(&soundChannel2, audioStep) * volumeChannel2 * audioVolume;
	}
}

/* # FILE LOADING */

char gameFilePath[256];
int gameCount = 0;

int loadScript(duk_context* ctx, char* filepath) {
	int success = 0;

	printf("Loading %s ...\n", filepath);

	char* fileBuffer = 0;
	long length;
	FILE* f = fopen(filepath, "r");

	if (f) {
		fseek(f, 0, SEEK_END);

		length = ftell(f);

		fseek(f, 0, SEEK_SET);

		// length +1 to make sure there's room for null terminator
		fileBuffer = malloc(length + 1);

		if (fileBuffer) {
			// replace seek length with read length
			length = fread(fileBuffer, 1, length, f);

			// ensure null terminator
			fileBuffer[length] = '\0';
		}

		fclose(f);
	}

	if (fileBuffer) {
		duk_push_lstring(ctx, (const char *) fileBuffer, (duk_size_t) length);

		if (duk_peval(ctx) != 0) {
			printf("Load Script Error: %s\n", duk_safe_to_string(ctx, -1));
		}
		else {
			success = 1;
		}

		duk_pop(ctx);
	}

	return success;
}

int loadFile(duk_context* ctx, char* filepath, char* variableName) {
	int success = 0;

	printf("Loading %s ...\n", filepath);

	char* fileBuffer = 0;
	long length;
	FILE* f = fopen(filepath, "r");

	if (f) {
		fseek(f, 0, SEEK_END);

		length = ftell(f);

		fseek(f, 0, SEEK_SET);

		// length +1 to make sure there's room for null terminator
		fileBuffer = malloc(length + 1);

		if (fileBuffer) {
			// replace seek length with read length
			length = fread(fileBuffer, 1, length, f);

			// ensure null terminator
			fileBuffer[length] = '\0';
		}

		fclose(f);
	}

	if (fileBuffer) {
		duk_push_lstring(ctx, (const char *) fileBuffer, (duk_size_t) length);
		duk_put_global_string(ctx, variableName);
		success = 1;
	}

	return success;
}

int loadEmbeddedScript(duk_context* ctx, char* fileStr) {
	int success = 1;

	if (duk_peval_string(ctx, fileStr) != 0) {
		printf("Load Embedded Script Error: %s\n", duk_safe_to_string(ctx, -1));
		success = 0;
	}
	duk_pop(ctx);

	return success;
}

int loadEmbeddedFile(duk_context* ctx, char* fileStr, char* variableName) {
	duk_push_string(ctx, fileStr);
	duk_put_global_string(ctx, variableName);

	return 1;
}

/* # BITSY SYSTEM API IMPLEMENTATION */

/* ## CONSTANTS */

// memory blocks
#define BITSY_VIDEO 0
#define BITSY_TEXTBOX 1
#define BITSY_MAP1 2
#define BITSY_MAP2 3
#define BITSY_SOUND1 4
#define BITSY_SOUND2 5

#define BITSY_TILE_START 6

// graphics modes
#define BITSY_GFX_VIDEO 0
#define BITSY_GFX_MAP 1

// text modes
#define BITSY_TXT_HIREZ 0
#define BITSY_TXT_LOREZ 1

// size
#define BITSY_TILE_SIZE 8
#define BITSY_MAP_SIZE 16
#define BITSY_VIDEO_SIZE 128

// button codes
#define BITSY_BTN_UP 0
#define BITSY_BTN_DOWN 1
#define BITSY_BTN_LEFT 2
#define BITSY_BTN_RIGHT 3
#define BITSY_BTN_OK 4
#define BITSY_BTN_MENU 5

// pulse waves
#define BITSY_PULSE_1_8 0
#define BITSY_PULSE_1_4 1
#define BITSY_PULSE_1_2 2

/* ## IO */

/* `bitsy.log(message)`
 *
 * Writes the string `message` to the debug console.
 */
duk_ret_t bitsyLog(duk_context* ctx) {
#ifdef ENABLE_BITSY_LOG
	const char* printStr;
	printStr = duk_safe_to_string(ctx, 0);

	printf("bitsy::%s\n", printStr);
#endif

	return 0;
}

/* `bitsy.button(code)`
 *
 * Returns `true` if the button referred to by `code` is held down. Otherwise it returns `false`.
 */
duk_ret_t bitsyButton(duk_context* ctx) {
	int buttonCode = duk_get_int(ctx, 0);

	int isAnyAlt = (isButtonLAlt || isButtonRAlt);
	int isAnyCtrl = (isButtonLCtrl || isButtonRCtrl);
	int isCtrlPlusR = isAnyCtrl && isButtonR;
	int isPadFaceButton = isButtonPadA || isButtonPadB || isButtonPadX || isButtonPadY;

	if (buttonCode == 0) {
		duk_push_boolean(ctx, isButtonUp || isButtonW || isButtonPadUp);
	}
	else if (buttonCode == 1) {
		duk_push_boolean(ctx, isButtonDown || isButtonS || isButtonPadDown);
	}
	else if (buttonCode == 2) {
		duk_push_boolean(ctx, isButtonLeft || isButtonA || isButtonPadLeft);
	}
	else if (buttonCode == 3) {
		duk_push_boolean(ctx, isButtonRight || isButtonD || isButtonPadRight);
	}
	else if (buttonCode == 4) {
		duk_push_boolean(ctx, isButtonSpace || (isButtonReturn && !isAnyAlt) || isPadFaceButton);
	}
	else if (buttonCode == 5) {
		duk_push_boolean(ctx, isButtonEscape || isCtrlPlusR || isButtonPadStart);
	}
	else {
		duk_push_boolean(ctx, 0);
	}

	return 1;
}

/* `bitsy.getGameData()`
 * 
 * Returns the game data as a string.
 */
duk_ret_t bitsyGetGameData(duk_context* ctx) {
	duk_peval_string(ctx, "__bitsybox_game_data__");

	return 1;
}

/* `bitsy.getFontData()`
 *
 * Returns the default font data as a string.
 */
duk_ret_t bitsyGetFontData(duk_context* ctx) {
	duk_peval_string(ctx, "__bitsybox_default_font__");

	return 1;
}

/* ## GRAPHICS */

/* `bitsy.graphicsMode(mode)`
 *
 * Sets the current graphics display `mode`, and also returns the current graphics mode.
 * If no mode input is given, just returns the current mode without changing it.
 */
duk_ret_t bitsyGraphicsMode(duk_context* ctx) {
	// set the graphics mode if there is an input mode
	if (duk_get_top(ctx) >= 1) {
		int prevGraphicsMode = curGraphicsMode;
		curGraphicsMode = duk_get_int(ctx, 0);

		if (curGraphicsMode != prevGraphicsMode) {
			shouldRenderTextures = 1;
		}
	}

	// return the current graphics mode
	duk_push_int(ctx, curGraphicsMode);

	return 1;
}

/* `bitsy.textMode(mode)`
 *
 * Sets the current text display `mode`, and also returns the current text mode.
 * If no `mode` input is given, just returns the current mode without changing it.
 */
duk_ret_t bitsyTextMode(duk_context* ctx) {
	// set the text mode if there is an input mode
	if (duk_get_top(ctx) >= 1) {
		int prevTextMode =  curTextMode;
		curTextMode = duk_get_int(ctx, 0);

		// update the textbox render scale
		textboxRenderScale = (curTextMode == BITSY_TXT_LOREZ) ? 4 : 2;

		if (curTextMode != prevTextMode) {
			shouldRenderTextures = 1;
		}
	}

	// return the current text mode
	duk_push_int(ctx, curTextMode);

	return 1;
}

/* `bitsy.color(color, r, g, b)`
 *
 * Sets the color in the system palette at index `color` to a color defined by the color parameters 
 * `r` (red), `g` (green), and `b` (blue). These values must be between 0 and 255. 
 * (For example, `bitsy.color(2, 0, 0, 0)` sets the color at index 2 to black and 
 * `bitsy.color(2, 255, 255, 255)` set the color at the same index to white.)
 */
duk_ret_t bitsyColor(duk_context* ctx) {
	int paletteIndex = duk_get_int(ctx, 0);
	int r = duk_get_int(ctx, 1);
	int g = duk_get_int(ctx, 2);
	int b = duk_get_int(ctx, 3);

	systemPalette[paletteIndex] = (Color) { r, g, b };

	// printf("bitsyColor %i - %i %i %i\n", paletteIndex, r, g, b);

	shouldRenderTextures = 1;

	return 0;
}

/* `bitsy.tile()`
 *
 * Allocates a new tile and returns its memory block location.
 */
duk_ret_t bitsyTile(duk_context* ctx) {
	// search the tile texture array for an empty entry
	int tileIndex = BITSY_TILE_START;
	while (tileIndex < TEXTURE_MAX && textures[tileIndex] != NULL) {
		tileIndex++;
	}

	// return nothing if there is no available space
	if (tileIndex >= TEXTURE_MAX) {
		return 0;
	}

	textures[tileIndex] = SDL_CreateTexture(
		renderer,
		SDL_PIXELFORMAT_RGB888,
		SDL_TEXTUREACCESS_TARGET,
		(BITSY_TILE_SIZE * renderScale),
		(BITSY_TILE_SIZE * renderScale));
	allocateMemoryBlock(tileIndex, BITSY_TILE_SIZE * BITSY_TILE_SIZE);

	duk_push_int(ctx, tileIndex);

	return 1;
}

/* `bitsy.delete(tile)`
 *
 * Deletes the tile at the specified memory block location.
 */
duk_ret_t bitsyDelete(duk_context* ctx) {
	int tile = duk_get_int(ctx, 0);
	if (tile >= BITSY_TILE_START && tile < TEXTURE_MAX && textures[tile] != NULL) {
		printf("BITSY DELETE %i\n", tile);

		// destroy the texture & nullify the reference to it
		SDL_DestroyTexture(textures[tile]);
		textures[tile] = NULL;

		// free the associated system memory
		freeMemoryBlock(tile);
	}

	return 0;
}

/* `bitsy.fill(block, value)`
 *
 * Fills an entire memory `block` with a number `value`. 
 * Can be used to clear blocks such as video memory, tilemap memory, and tile memory.
 */
duk_ret_t bitsyFill(duk_context* ctx) {
	int block = duk_get_int(ctx, 0);
	int value = duk_get_int(ctx, 1);

	// verify valid block
	if (isMemoryBlockValid(block)) {
		// verify valid data
		if (value >= 0 && value < 256) {
			// everything is ok - set the data!
			for (int i = 0; i < memory[block].size; i++) {
				memory[block].data[i] = value;
			}

			if (textures[block] != NULL) {
				shouldRenderTextures = 1;
			}
		}
	}

	return 0;
}

/* `bitsy.set(block, index, value)`
 *
 * Sets the value at `index` within a memory `block` with a number `value`.
 */
duk_ret_t bitsySet(duk_context* ctx) {
	int block = duk_get_int(ctx, 0);
	int index = duk_get_int(ctx, 1);
	int value = duk_get_int(ctx, 2);

	// printf("BITSY SET %i %i %i - GFX %i\n", block, index, value, curGraphicsMode);

	// verify valid block
	if (isMemoryBlockValid(block)) {
		// verify valid location in block
		if (index >= 0 && index < memory[block].size) {
			// verify valid data
			if (value >= 0 && value < 256) {
				// everything is ok - set the data!
				memory[block].data[index] = value;

				if (textures[block] != NULL) {
					shouldRenderTextures = 1;
				}
			}
		}
	}

	return 0;
}

/* `bitsy.textbox(visible, x, y, w, h)`
 *
 * Updates the textbox display settings.
 * If `visible` is `true` the textbox is rendered, otherwise it's hidden.
 * The textbox's position (relative the main display's coordinate space)
 * is defined by `x` and `y`. And the size of the textbox
 * (in its internal resolution) is defined by `w` (width) and `h` (height).
 * Omitted parameters are unchanged (For example, you can just reveal
 * the textbox without changing its position and size using `bitsy.textbox(true)`).
 */
duk_ret_t bitsyTextbox(duk_context* ctx) {
	if (duk_get_top(ctx) >= 1) {
		isTextboxVisible = duk_get_boolean(ctx, 0);
	}

	if (duk_get_top(ctx) >= 3) {
		textboxX = duk_get_int(ctx, 1);
		textboxY = duk_get_int(ctx, 2);
	}

	if (duk_get_top(ctx) >= 5) {
		textboxWidth = duk_get_int(ctx, 3);
		textboxHeight = duk_get_int(ctx, 4);

		// create a new texture when the size changes
		textures[BITSY_TEXTBOX] = SDL_CreateTexture(
			renderer,
			SDL_PIXELFORMAT_RGB888,
			SDL_TEXTUREACCESS_TARGET,
			(textboxWidth * textboxRenderScale),
			(textboxHeight * textboxRenderScale));
		allocateMemoryBlock(BITSY_TEXTBOX, textboxWidth * textboxHeight);
	}

	return 0;
}

/* ## SOUND */

/* `bitsy.sound(channel, duration, frequency, volume, pulse)`
 *
 * Updates all audio settings for one sound `channel` (either `bitsy.SOUND1` or `bitsy.SOUND2`).
 * The `duration` is in milliseconds, `frequency` is in decihertz (dHz), `volume` must be between 0 and 15,
 * and `pulse` is one of the pulse wave constants.
 */
duk_ret_t bitsySound(duk_context* ctx) {
	if (duk_get_top(ctx) >= 1) {
		int channel = duk_get_int(ctx, 0);
		float frequency = 0.0f;

		switch (channel) {
			case BITSY_SOUND1:
				if (duk_get_top(ctx) >= 2) {
					// duration is passed in as milliseconds, but here we convert it
					// to samples since that is easier to sync with the SDL audio system
					durationChannel1 = floor((duk_get_int(ctx, 1) / 1000.0f) * AUDIO_SAMPLE_RATE);
				}

				if (duk_get_top(ctx) >= 3) {
					frequency = duk_get_int(ctx, 2) / 100.0f;
				}

				if (duk_get_top(ctx) >= 4) {
					volumeChannel1 = duk_get_int(ctx, 3) / 15.0f;
				}

				if (duk_get_top(ctx) >= 5) {
					dutyChannel1 = duk_get_int(ctx, 4);
				}

				switch (dutyChannel1) {
					case BITSY_PULSE_1_8:
						soundChannel1 = wave(frequency, (1.0f / 8.0f));
						break;
					case BITSY_PULSE_1_4:
						soundChannel1 = wave(frequency, (1.0f / 4.0f));
						break;
					case BITSY_PULSE_1_2:
						soundChannel1 = wave(frequency, (1.0f / 2.0f));
						break;
				}
				break;
			case BITSY_SOUND2:
				if (duk_get_top(ctx) >= 2) {
					// duration is passed in as milliseconds, but here we convert it
					// to samples since that is easier to sync with the SDL audio system
					durationChannel2 = floor((duk_get_int(ctx, 1) / 1000.0f) * AUDIO_SAMPLE_RATE);
				}

				if (duk_get_top(ctx) >= 3) {
					frequency = duk_get_int(ctx, 2) / 100.0f;
				}

				if (duk_get_top(ctx) >= 4) {
					volumeChannel2 = duk_get_int(ctx, 3) / 15.0f;
				}

				if (duk_get_top(ctx) >= 5) {
					dutyChannel2 = duk_get_int(ctx, 4);
				}

				switch (dutyChannel2) {
					case BITSY_PULSE_1_8:
						soundChannel2 = wave(frequency, (1.0f / 8.0f));
						break;
					case BITSY_PULSE_1_4:
						soundChannel2 = wave(frequency, (1.0f / 4.0f));
						break;
					case BITSY_PULSE_1_2:
						soundChannel2 = wave(frequency, (1.0f / 2.0f));
						break;
				}
				break;
		}
	}

	return 0;
}

/* `bitsy.frequency(channel, frequency)`
 *
 * Sets the `frequency` for one sound `channel`. Units are decihertz (dHz).
 */
duk_ret_t bitsyFrequency(duk_context* ctx) {
	int channel = duk_get_int(ctx, 0);
	float frequency = duk_get_int(ctx, 1) / 100.0f;

	switch (channel) {
		case BITSY_SOUND1:
			switch (dutyChannel1) {
				case BITSY_PULSE_1_8:
					soundChannel1 = wave(frequency, (1.0f / 8.0f));
					break;
				case BITSY_PULSE_1_4:
					soundChannel1 = wave(frequency, (1.0f / 4.0f));
					break;
				case BITSY_PULSE_1_2:
					soundChannel1 = wave(frequency, (1.0f / 2.0f));
					break;
			}
			break;
		case BITSY_SOUND2:
			switch (dutyChannel2) {
				case BITSY_PULSE_1_8:
					soundChannel2 = wave(frequency, (1.0f / 8.0f));
					break;
				case BITSY_PULSE_1_4:
					soundChannel2 = wave(frequency, (1.0f / 4.0f));
					break;
				case BITSY_PULSE_1_2:
					soundChannel2 = wave(frequency, (1.0f / 2.0f));
					break;
			}
			break;
	}

	return 0;
}

/* `bitsy.volume(channel, volume)`
 *
 * Sets the `volume` for one sound `channel`. Volume must be between 0 and 15 (inclusive).
 */
duk_ret_t bitsyVolume(duk_context* ctx) {
	int channel = duk_get_int(ctx, 0);
	int volume = duk_get_int(ctx, 1);

	switch (channel) {
		case BITSY_SOUND1:
			volumeChannel1 = volume / 15.0f;
			break;
		case BITSY_SOUND2:
			volumeChannel2 = volume / 15.0f;
			break;
	}

	return 0;
}

/* ## EVENTS */

/* `bitsy.loop(fn)`
 *
 * The system will call function `fn` on every update loop. 
 * It will attempt to run at 60fps, but `fn` will also receive 
 * an input parameter `dt` with the delta time since the previous loop (in milliseconds).
 */
duk_ret_t bitsyLoop(duk_context* ctx) {
	duk_put_global_string(ctx, "__bitsybox_on_update__");

	return 0;
}

void initBitsyInterface(duk_context* ctx) {
	// BITSY API v0.2

	// create system object
	duk_idx_t bitsySystemIdx = duk_push_object(ctx);

	// CONSTANTS

	// memory blocks
	duk_push_int(ctx, BITSY_VIDEO);
	duk_put_prop_string(ctx, bitsySystemIdx, "VIDEO");

	duk_push_int(ctx, BITSY_TEXTBOX);
	duk_put_prop_string(ctx, bitsySystemIdx, "TEXTBOX");

	duk_push_int(ctx, BITSY_MAP1);
	duk_put_prop_string(ctx, bitsySystemIdx, "MAP1");

	duk_push_int(ctx, BITSY_MAP2);
	duk_put_prop_string(ctx, bitsySystemIdx, "MAP2");

	duk_push_int(ctx, BITSY_SOUND1);
	duk_put_prop_string(ctx, bitsySystemIdx, "SOUND1");

	duk_push_int(ctx, BITSY_SOUND2);
	duk_put_prop_string(ctx, bitsySystemIdx, "SOUND2");

	// graphics modes
	duk_push_int(ctx, BITSY_GFX_VIDEO);
	duk_put_prop_string(ctx, bitsySystemIdx, "GFX_VIDEO");

	duk_push_int(ctx, BITSY_GFX_MAP);
	duk_put_prop_string(ctx, bitsySystemIdx, "GFX_MAP");

	// text modes
	duk_push_int(ctx, BITSY_TXT_HIREZ);
	duk_put_prop_string(ctx, bitsySystemIdx, "TXT_HIREZ");

	duk_push_int(ctx, BITSY_TXT_LOREZ);
	duk_put_prop_string(ctx, bitsySystemIdx, "TXT_LOREZ");

	// sizes
	duk_push_int(ctx, BITSY_TILE_SIZE);
	duk_put_prop_string(ctx, bitsySystemIdx, "TILE_SIZE");

	duk_push_int(ctx, BITSY_MAP_SIZE);
	duk_put_prop_string(ctx, bitsySystemIdx, "MAP_SIZE");

	duk_push_int(ctx, BITSY_VIDEO_SIZE);
	duk_put_prop_string(ctx, bitsySystemIdx, "VIDEO_SIZE");

	// button codes
	duk_push_int(ctx, BITSY_BTN_UP);
	duk_put_prop_string(ctx, bitsySystemIdx, "BTN_UP");

	duk_push_int(ctx, BITSY_BTN_DOWN);
	duk_put_prop_string(ctx, bitsySystemIdx, "BTN_DOWN");

	duk_push_int(ctx, BITSY_BTN_LEFT);
	duk_put_prop_string(ctx, bitsySystemIdx, "BTN_LEFT");

	duk_push_int(ctx, BITSY_BTN_RIGHT);
	duk_put_prop_string(ctx, bitsySystemIdx, "BTN_RIGHT");

	duk_push_int(ctx, BITSY_BTN_OK);
	duk_put_prop_string(ctx, bitsySystemIdx, "BTN_OK");

	duk_push_int(ctx, BITSY_BTN_MENU);
	duk_put_prop_string(ctx, bitsySystemIdx, "BTN_MENU");

	// pulse waves
	duk_push_int(ctx, BITSY_PULSE_1_8);
	duk_put_prop_string(ctx, bitsySystemIdx, "PULSE_1_8");

	duk_push_int(ctx, BITSY_PULSE_1_4);
	duk_put_prop_string(ctx, bitsySystemIdx, "PULSE_1_4");

	duk_push_int(ctx, BITSY_PULSE_1_2);
	duk_put_prop_string(ctx, bitsySystemIdx, "PULSE_1_2");

	// IO

	duk_push_c_function(ctx, bitsyLog, 1);
	duk_put_prop_string(ctx, bitsySystemIdx, "log");

	duk_push_c_function(ctx, bitsyButton, 1);
	duk_put_prop_string(ctx, bitsySystemIdx, "button");

	duk_push_c_function(ctx, bitsyGetGameData, 0);
	duk_put_prop_string(ctx, bitsySystemIdx, "getGameData");

	duk_push_c_function(ctx, bitsyGetFontData, 0);
	duk_put_prop_string(ctx, bitsySystemIdx, "getFontData");

	// GRAPHICS

	duk_push_c_function(ctx, bitsyGraphicsMode, DUK_VARARGS);
	duk_put_prop_string(ctx, bitsySystemIdx, "graphicsMode");

	duk_push_c_function(ctx, bitsyTextMode, DUK_VARARGS);
	duk_put_prop_string(ctx, bitsySystemIdx, "textMode");

	duk_push_c_function(ctx, bitsyColor, 4);
	duk_put_prop_string(ctx, bitsySystemIdx, "color");

	duk_push_c_function(ctx, bitsyTile, 0);
	duk_put_prop_string(ctx, bitsySystemIdx, "tile");

	duk_push_c_function(ctx, bitsyDelete, 1);
	duk_put_prop_string(ctx, bitsySystemIdx, "delete");

	duk_push_c_function(ctx, bitsyFill, 2);
	duk_put_prop_string(ctx, bitsySystemIdx, "fill");

	duk_push_c_function(ctx, bitsySet, 3);
	duk_put_prop_string(ctx, bitsySystemIdx, "set");

	duk_push_c_function(ctx, bitsyTextbox, DUK_VARARGS);
	duk_put_prop_string(ctx, bitsySystemIdx, "textbox");

	// SOUND

	duk_push_c_function(ctx, bitsySound, DUK_VARARGS);
	duk_put_prop_string(ctx, bitsySystemIdx, "sound");

	duk_push_c_function(ctx, bitsyFrequency, 2);
	duk_put_prop_string(ctx, bitsySystemIdx, "frequency");

	duk_push_c_function(ctx, bitsyVolume, 2);
	duk_put_prop_string(ctx, bitsySystemIdx, "volume");

	// EVENTS

	duk_push_c_function(ctx, bitsyLoop, 1);
	duk_put_prop_string(ctx, bitsySystemIdx, "loop");

	// BITSY SYSTEM

	// assign name to system object
	duk_put_global_string(ctx, "bitsy");
}

/* # INITIALIZATION */

void resetMemoryAndTextures() {
	// delete any textures
	for (int i = 0; i < TEXTURE_MAX; i++) {
		if (textures[i] != NULL) {
			SDL_DestroyTexture(textures[i]);
			textures[i] = NULL;
		}
	}

	// initialize system memory
	initializeMemoryBlocks();

	// video mode texture
	textures[BITSY_VIDEO] = SDL_CreateTexture(
		renderer,
		SDL_PIXELFORMAT_RGB888,
		SDL_TEXTUREACCESS_TARGET,
		(BITSY_VIDEO_SIZE * renderScale),
		(BITSY_VIDEO_SIZE * renderScale));
	allocateMemoryBlock(BITSY_VIDEO, BITSY_VIDEO_SIZE * BITSY_VIDEO_SIZE);

	// map mode textures
	textures[BITSY_MAP1] = SDL_CreateTexture(
		renderer,
		SDL_PIXELFORMAT_RGB888,
		SDL_TEXTUREACCESS_TARGET,
		(BITSY_VIDEO_SIZE * renderScale),
		(BITSY_VIDEO_SIZE * renderScale));
	allocateMemoryBlock(BITSY_MAP1, BITSY_MAP_SIZE * BITSY_MAP_SIZE);

	textures[BITSY_MAP2] = SDL_CreateTexture(
		renderer,
		SDL_PIXELFORMAT_RGBA8888,
		SDL_TEXTUREACCESS_TARGET,
		(BITSY_VIDEO_SIZE * renderScale),
		(BITSY_VIDEO_SIZE * renderScale));
	allocateMemoryBlock(BITSY_MAP2, BITSY_MAP_SIZE * BITSY_MAP_SIZE);
	// enable alpha blending for the foreground tile map texture
	SDL_SetTextureBlendMode(textures[BITSY_MAP2], SDL_BLENDMODE_BLEND);

	// create textbox texture
	textures[BITSY_TEXTBOX] = SDL_CreateTexture(
		renderer,
		SDL_PIXELFORMAT_RGB888,
		SDL_TEXTUREACCESS_TARGET,
		(textboxWidth * textboxRenderScale),
		(textboxHeight * textboxRenderScale));
	allocateMemoryBlock(BITSY_TEXTBOX, textboxWidth * textboxHeight);
}

void loadEngine(duk_context* ctx) {
#ifdef BUILD_DEBUG
	// load engine
	shouldContinue = shouldContinue && loadScript(ctx, "bitsy/engine/world.js");
	shouldContinue = shouldContinue && loadScript(ctx, "bitsy/engine/sound.js");
	shouldContinue = shouldContinue && loadScript(ctx, "bitsy/engine/font.js");
	shouldContinue = shouldContinue && loadScript(ctx, "bitsy/engine/transition.js");
	shouldContinue = shouldContinue && loadScript(ctx, "bitsy/engine/script.js");
	shouldContinue = shouldContinue && loadScript(ctx, "bitsy/engine/dialog.js");
	shouldContinue = shouldContinue && loadScript(ctx, "bitsy/engine/renderer.js");
	shouldContinue = shouldContinue && loadScript(ctx, "bitsy/engine/bitsy.js");
	// load default font
	shouldContinue = shouldContinue && loadFile(ctx, "bitsy/font/ascii_small.bitsyfont", "__bitsybox_default_font__");
#else
	// load engine
	shouldContinue = shouldContinue && loadEmbeddedScript(ctx, world_js);
	shouldContinue = shouldContinue && loadEmbeddedScript(ctx, sound_js);
	shouldContinue = shouldContinue && loadEmbeddedScript(ctx, font_js);
	shouldContinue = shouldContinue && loadEmbeddedScript(ctx, transition_js);
	shouldContinue = shouldContinue && loadEmbeddedScript(ctx, script_js);
	shouldContinue = shouldContinue && loadEmbeddedScript(ctx, dialog_js);
	shouldContinue = shouldContinue && loadEmbeddedScript(ctx, renderer_js);
	shouldContinue = shouldContinue && loadEmbeddedScript(ctx, bitsy_js);
	// load default font
	shouldContinue = shouldContinue && loadEmbeddedFile(ctx, ascii_small_bitsyfont, "__bitsybox_default_font__");
#endif
}

void initSystem(duk_context* ctx) {
	resetMemoryAndTextures();
	initBitsyInterface(ctx);
	loadEngine(ctx);
}

/* # UPDATE */

void updateInput() {
	if (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT) {
			shouldContinue = 0;
		}
		else if (event.type == SDL_WINDOWEVENT) {
			if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
				didWindowResizeThisFrame = 1;
			}
		}
		else if (event.type == SDL_KEYDOWN) {
			if (event.key.keysym.sym == SDLK_UP) {
				isButtonUp = 1;
			}
			else if (event.key.keysym.sym == SDLK_DOWN) {
				isButtonDown = 1;
			}
			else if (event.key.keysym.sym == SDLK_LEFT) {
				isButtonLeft = 1;
			}
			else if (event.key.keysym.sym == SDLK_RIGHT) {
				isButtonRight = 1;
			}
			else if (event.key.keysym.sym == SDLK_w) {
				isButtonW = 1;
			}
			else if (event.key.keysym.sym == SDLK_a) {
				isButtonA = 1;
			}
			else if (event.key.keysym.sym == SDLK_s) {
				isButtonS = 1;
			}
			else if (event.key.keysym.sym == SDLK_d) {
				isButtonD = 1;
			}
			else if (event.key.keysym.sym == SDLK_r) {
				isButtonR = 1;
			}
			else if (event.key.keysym.sym == SDLK_SPACE) {
				isButtonSpace = 1;
			}
			else if (event.key.keysym.sym == SDLK_RETURN) {
				isButtonReturn = 1;
			}
			else if (event.key.keysym.sym == SDLK_ESCAPE) {
				isButtonEscape = 1;
			}
			else if (event.key.keysym.sym == SDLK_LCTRL) {
				isButtonLCtrl = 1;
			}
			else if (event.key.keysym.sym == SDLK_RCTRL) {
				isButtonRCtrl = 1;
			}
			else if (event.key.keysym.sym == SDLK_LALT) {
				isButtonLAlt = 1;
			}
			else if (event.key.keysym.sym == SDLK_RALT) {
				isButtonRAlt = 1;
			}
		}
		else if (event.type == SDL_KEYUP) {
			if (event.key.keysym.sym == SDLK_UP) {
				isButtonUp = 0;
			}
			else if (event.key.keysym.sym == SDLK_DOWN) {
				isButtonDown = 0;
			}
			else if (event.key.keysym.sym == SDLK_LEFT) {
				isButtonLeft = 0;
			}
			else if (event.key.keysym.sym == SDLK_RIGHT) {
				isButtonRight = 0;
			}
			else if (event.key.keysym.sym == SDLK_RIGHT) {
				isButtonRight = 0;
			}
			else if (event.key.keysym.sym == SDLK_w) {
				isButtonW = 0;
			}
			else if (event.key.keysym.sym == SDLK_a) {
				isButtonA = 0;
			}
			else if (event.key.keysym.sym == SDLK_s) {
				isButtonS = 0;
			}
			else if (event.key.keysym.sym == SDLK_d) {
				isButtonD = 0;
			}
			else if (event.key.keysym.sym == SDLK_r) {
				isButtonR = 0;
			}
			else if (event.key.keysym.sym == SDLK_SPACE) {
				isButtonSpace = 0;
			}
			else if (event.key.keysym.sym == SDLK_RETURN) {
				isButtonReturn = 0;
			}
			else if (event.key.keysym.sym == SDLK_ESCAPE) {
				isButtonEscape = 0;
			}
			else if (event.key.keysym.sym == SDLK_LCTRL) {
				isButtonLCtrl = 0;
			}
			else if (event.key.keysym.sym == SDLK_RCTRL) {
				isButtonRCtrl = 0;
			}
			else if (event.key.keysym.sym == SDLK_LALT) {
				isButtonLAlt = 0;
			}
			else if (event.key.keysym.sym == SDLK_RALT) {
				isButtonRAlt = 0;
			}
		}
		else if (event.type == SDL_CONTROLLERDEVICEADDED) {
			SDL_GameControllerOpen(event.cdevice.which);
		}
		else if (event.type == SDL_CONTROLLERBUTTONDOWN) {
			if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP) {
				isButtonPadUp = 1;
			}
			else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) {
				isButtonPadDown = 1;
			}
			else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) {
				isButtonPadLeft = 1;
			}
			else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
				isButtonPadRight = 1;
			}
			else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
				isButtonPadA = 1;
			}
			else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_B) {
				isButtonPadB = 1;
			}
			else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_X) {
				isButtonPadX = 1;
			}
			else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_Y) {
				isButtonPadY = 1;
			}
			else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_START) {
				isButtonPadStart = 1;
			}
		}
		else if (event.type == SDL_CONTROLLERBUTTONUP) {
			if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP) {
				isButtonPadUp = 0;
			}
			else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) {
				isButtonPadDown = 0;
			}
			else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) {
				isButtonPadLeft = 0;
			}
			else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
				isButtonPadRight = 0;
			}
			else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
				isButtonPadA = 0;
			}
			else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_B) {
				isButtonPadB = 0;
			}
			else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_X) {
				isButtonPadX = 0;
			}
			else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_Y) {
				isButtonPadY = 0;
			}
			else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_START) {
				isButtonPadStart = 0;
			}
		}
	}

	// get mouse state
	int windowMouseX = 0;
	int windowMouseY = 0;
	isLeftMouseDown = SDL_GetMouseState(&windowMouseX, &windowMouseY) & SDL_BUTTON(1);

	// normalize mouse position to bitsy scale
	int screenSize = (windowWidth <= windowHeight) ? windowWidth : windowHeight;
	int screenLeft = (windowWidth / 2) - (screenSize / 2);
	int screenTop = (windowHeight / 2) - (screenSize / 2);
	float pixelRatio = ((float) BITSY_VIDEO_SIZE) / ((float) screenSize);
	mouseX = floor((windowMouseX - screenLeft) * pixelRatio);
	mouseY = floor((windowMouseY - screenTop) * pixelRatio);

	// toggle fullscreen
	int prevFrameFullscreenShortcut = isFullscreenShortcut;
	isFullscreenShortcut = (isButtonLAlt || isButtonRAlt) && isButtonReturn;

	if (isFullscreenShortcut && !prevFrameFullscreenShortcut) {
		isFullscreen = (isFullscreen == 0 ? 1 : 0);

		SDL_ShowCursor(!isFullscreen);
		SDL_SetWindowFullscreen(window, isFullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);

		if (!isFullscreen) {
			SDL_SetWindowSize(window, (BITSY_VIDEO_SIZE * renderScale), (BITSY_VIDEO_SIZE * renderScale));
		}

		didWindowResizeThisFrame = 1;
	}
}

void renderVideoTexture() {
	int backgroundColorIndex = 16;
	Color bgColor = systemPalette[backgroundColorIndex];

	// will changing these a bunch cause a perf issue?
	SDL_SetRenderTarget(renderer, textures[BITSY_VIDEO]);

	// fill background
	SDL_SetRenderDrawColor(renderer, bgColor.r, bgColor.g, bgColor.b, 0x00);
	SDL_Rect videoFillRect = { 0, 0, (BITSY_VIDEO_SIZE * renderScale), (BITSY_VIDEO_SIZE * renderScale), };
	SDL_RenderFillRect(renderer, &videoFillRect);

	// draw pixels
	if (!isMemoryBlockEmpty(BITSY_VIDEO)) {
		MemoryBlock videoMemory = memory[BITSY_VIDEO];
		for (int i = 0; i < videoMemory.size; i++) {
			int pixelColorIndex = videoMemory.data[i];

			// skip background color since that's the fill color
			if (pixelColorIndex >= 0 && pixelColorIndex != backgroundColorIndex) {
				int pixelX = i % BITSY_VIDEO_SIZE;
				int pixelY = i / BITSY_VIDEO_SIZE;

				Color pixelColor = systemPalette[pixelColorIndex];
				SDL_Rect pixelRect = { (pixelX * renderScale), (pixelY * renderScale), renderScale, renderScale, };
				SDL_SetRenderDrawColor(renderer, pixelColor.r, pixelColor.g, pixelColor.b, 0x00);
				SDL_RenderFillRect(renderer, &pixelRect);
			}
		}		
	}
}

void renderTileTextures() {
	int backgroundColorIndex = 16;
	Color bgColor = systemPalette[backgroundColorIndex];

	// render tiles
	for (int tileIndex = BITSY_TILE_START; tileIndex < MEMORY_BLOCK_MAX; tileIndex++) {
		if (textures[tileIndex] != NULL) {
			SDL_SetRenderTarget(renderer, textures[tileIndex]);

			SDL_SetRenderDrawColor(renderer, bgColor.r, bgColor.g, bgColor.b, 0xFF);
			SDL_Rect tileFillRect = { 0, 0, (BITSY_TILE_SIZE * renderScale), (BITSY_TILE_SIZE * renderScale), };
			SDL_RenderFillRect(renderer, &tileFillRect);

			if (!isMemoryBlockEmpty(tileIndex)) {
				MemoryBlock tileMemory = memory[tileIndex];
				for (int pixelIndex = 0; pixelIndex < tileMemory.size; pixelIndex++) {
					int pixelColorIndex = tileMemory.data[pixelIndex];

					// skip background color since that's the fill color
					if (pixelColorIndex >= 0 && pixelColorIndex != backgroundColorIndex) {
						Color pixelColor = systemPalette[pixelColorIndex];

						// convert index to 2d coords
						int pixelX = pixelIndex % BITSY_TILE_SIZE;
						int pixelY = pixelIndex / BITSY_TILE_SIZE;

						SDL_Rect pixelRect = { (pixelX * renderScale), (pixelY * renderScale), renderScale, renderScale, };
						SDL_SetRenderDrawColor(renderer, pixelColor.r, pixelColor.g, pixelColor.b, 0xFF);
						SDL_RenderFillRect(renderer, &pixelRect);
					}
				}
			}
		}
	}

	// render tilemap 1
	SDL_SetRenderTarget(renderer, textures[BITSY_MAP1]);

	SDL_SetRenderDrawColor(renderer, bgColor.r, bgColor.g, bgColor.b, 0x00);
	SDL_Rect map1FillRect = { 0, 0, (BITSY_VIDEO_SIZE * renderScale), (BITSY_VIDEO_SIZE * renderScale), };
	SDL_RenderFillRect(renderer, &map1FillRect);

	if (!isMemoryBlockEmpty(BITSY_MAP1)) {
		MemoryBlock map1Memory = memory[BITSY_MAP1];
		for (int i = 0; i < map1Memory.size; i++) {
			int tileId = map1Memory.data[i];

			// convert index to 2d *tile* coords
			int tileX = i % BITSY_MAP_SIZE;
			int tileY = i / BITSY_MAP_SIZE;

			SDL_Rect tileRect = {
				(tileX * BITSY_TILE_SIZE * renderScale),
				(tileY * BITSY_TILE_SIZE * renderScale),
				(BITSY_TILE_SIZE * renderScale),
				(BITSY_TILE_SIZE * renderScale),
			};

			if (tileId >= BITSY_TILE_START && tileId < TEXTURE_MAX && textures[tileId] != NULL) {
				SDL_RenderCopy(renderer, textures[tileId], NULL, &tileRect);
			}
		}
	}

	// render tilemap 2
	SDL_SetRenderTarget(renderer, textures[BITSY_MAP2]);

	SDL_SetRenderDrawColor(renderer, bgColor.r, bgColor.g, bgColor.b, 0x00);
	SDL_Rect map2FillRect = { 0, 0, (BITSY_VIDEO_SIZE * renderScale), (BITSY_VIDEO_SIZE * renderScale), };
	SDL_RenderFillRect(renderer, &map2FillRect);

	if (!isMemoryBlockEmpty(BITSY_MAP2)) {
		MemoryBlock map2Memory = memory[BITSY_MAP2];
		for (int i = 0; i < map2Memory.size; i++) {
			int tileId = map2Memory.data[i];

			// convert index to 2d *tile* coords
			int tileX = i % BITSY_MAP_SIZE;
			int tileY = i / BITSY_MAP_SIZE;

			SDL_Rect tileRect = {
				(tileX * BITSY_TILE_SIZE * renderScale),
				(tileY * BITSY_TILE_SIZE * renderScale),
				(BITSY_TILE_SIZE * renderScale),
				(BITSY_TILE_SIZE * renderScale),
			};

			if (tileId >= BITSY_TILE_START && tileId < TEXTURE_MAX && textures[tileId] != NULL) {
				SDL_RenderCopy(renderer, textures[tileId], NULL, &tileRect);
			}
		}
	}

	// render textbox
	SDL_SetRenderTarget(renderer, textures[BITSY_TEXTBOX]);

	Color textboxBgColor = systemPalette[0];
	SDL_SetRenderDrawColor(renderer, textboxBgColor.r, textboxBgColor.g, textboxBgColor.b, 0x00);
	SDL_Rect textboxFillRect = { 0, 0, (textboxWidth * textboxRenderScale), (textboxHeight * textboxRenderScale), };
	SDL_RenderFillRect(renderer, &textboxFillRect);

	if (!isMemoryBlockEmpty(BITSY_TEXTBOX)) {
		MemoryBlock textboxMemory = memory[BITSY_TEXTBOX];
		for (int i = 0; i < textboxMemory.size; i++) {
			int pixelColorIndex = textboxMemory.data[i];

			// since the texture is filled with color 0, we can skip pixels with that color
			if (pixelColorIndex > 0) {
				Color pixelColor = systemPalette[pixelColorIndex];

				// convert index to 2d coords
				int pixelX = i % textboxWidth;
				int pixelY = i / textboxWidth;

				SDL_Rect pixelRect = { (pixelX * textboxRenderScale), (pixelY * textboxRenderScale), textboxRenderScale, textboxRenderScale, };
				SDL_SetRenderDrawColor(renderer, pixelColor.r, pixelColor.g, pixelColor.b, 0x00);
				SDL_RenderFillRect(renderer, &pixelRect);
			}
		}
	}
}

void renderFrame() {
	int backgroundColorIndex = 16;
	Color bgColor = systemPalette[backgroundColorIndex];

	int screenSize = (windowWidth <= windowHeight) ? windowWidth : windowHeight;

	SDL_Rect screenRect = (SDL_Rect) {
		(windowWidth / 2) - (screenSize / 2),
		(windowHeight / 2) - (screenSize / 2),
		screenSize,
		screenSize,
	};

	// update textures
	if (shouldRenderTextures) {
		if (curGraphicsMode == BITSY_GFX_VIDEO) {
			renderVideoTexture();
		}
		else {
			renderTileTextures();
		}

		shouldRenderTextures = 0;
	}

	// render screen
	SDL_SetRenderTarget(renderer, NULL);

	// fill the screen with the background color
	SDL_SetRenderDrawColor(renderer, bgColor.r, bgColor.g, bgColor.b, 0x00);
	SDL_Rect windowRect = { 0, 0, windowWidth, windowHeight };
	SDL_RenderFillRect(renderer, &windowRect);

	if (curGraphicsMode == BITSY_GFX_VIDEO) {
		// copy the screen buffer texture into the renderer
		SDL_RenderCopy(renderer, textures[BITSY_VIDEO], NULL, &screenRect);
	}
	else {
		// draw tile map layers
		SDL_RenderCopy(renderer, textures[BITSY_MAP1], NULL, &screenRect);
		SDL_RenderCopy(renderer, textures[BITSY_MAP2], NULL, &screenRect);

		// draw textbox
		if (isTextboxVisible) {
			// calculate textbox dimensions in terms of the current window size
			float bitsyToScreenRatio = ((float) screenSize) / ((float) BITSY_VIDEO_SIZE);
			float textboxRatio = ((float) textboxRenderScale) / ((float) renderScale);
			SDL_Rect textboxRect = {
				screenRect.x + (textboxX * bitsyToScreenRatio),
				screenRect.y + (textboxY * bitsyToScreenRatio),
				(textboxWidth * textboxRatio * bitsyToScreenRatio),
				(textboxHeight * textboxRatio * bitsyToScreenRatio),
			};

			SDL_RenderCopy(renderer, textures[BITSY_TEXTBOX], NULL, &textboxRect);
		}
	}

	// show the frame
	SDL_RenderPresent(renderer);
}

void updateSystem(duk_context* ctx, int deltaTime) {
	// get latest input
	updateInput();

	// send the frame's delta time to the javascript VM
	duk_push_int(ctx, deltaTime);
	duk_put_global_string(ctx, "__bitsybox_delta_time__");

	// execute engine main loop
	if (duk_peval_string(ctx, "__bitsybox_on_update__(__bitsybox_delta_time__);") != 0) {
		printf("Update Bitsy Error: %s\n", duk_safe_to_string(ctx, -1));
	}
	duk_pop(ctx);

	// draw frame
	renderFrame();

	if (didWindowResizeThisFrame) {
		onWindowResize();
	}

	didWindowResizeThisFrame = 0;
}

/* # BITSYBOX MODES */

int shouldStartTuneDemo = 0;

void bootMenu() {
	SDL_SetWindowTitle(window, "BITSYBOX");

	duk_context* ctx = duk_create_heap_default();

	// load game files
	duk_peval_string(ctx, "__bitsybox_game_files__ = []");
	duk_pop(ctx);
	struct dirent* files;
	DIR* dir = opendir("./games");

	if (dir != NULL ) {
		while ((files = readdir(dir)) != NULL) {
			duk_push_string(ctx, files->d_name);
			duk_put_global_string(ctx, "__bitsybox_filename__");
			duk_peval_string(
				ctx,
				"var fileSplit = __bitsybox_filename__.split('.');"
				"if (fileSplit[fileSplit.length - 1] === 'bitsy') { __bitsybox_game_files__.push(__bitsybox_filename__); }");
			duk_pop(ctx);
		}

		closedir(dir);
	}

	// loop time
	int prevTime = SDL_GetTicks();
	int deltaTime = 0;

	int isBootFinished = 0;

	initSystem(ctx);

	// load boot menu
#ifdef BUILD_DEBUG
	shouldContinue = shouldContinue && loadScript(ctx, "boot/boot.js");
	shouldContinue = shouldContinue && loadFile(ctx, "boot/boot.bitsy", "__bitsybox_game_data__");
#else
	shouldContinue = shouldContinue && loadEmbeddedScript(ctx, boot_js);
	shouldContinue = shouldContinue && loadEmbeddedFile(ctx, boot_bitsy, "__bitsybox_game_data__");
#endif

	while (shouldContinue && !isBootFinished) {
		// update time
		deltaTime = SDL_GetTicks() - prevTime;
		prevTime = SDL_GetTicks();

		updateSystem(ctx, deltaTime);

		duk_peval_string(ctx, "__bitsybox_is_boot_finished__");
		isBootFinished = duk_get_boolean(ctx, -1);
		duk_pop(ctx);
	}

	if (isBootFinished) {
		duk_peval_string(ctx, "__bitsybox_selected_game_name__");
		SDL_SetWindowTitle(window, duk_get_string(ctx, -1));
		duk_pop(ctx);

		duk_peval_string(ctx, "__bitsybox_selected_game__");
		sprintf(gameFilePath, "./games/%s", duk_get_string(ctx, -1));
		duk_pop(ctx);

		duk_peval_string(ctx, "__bitsybox_game_files__.length");
		gameCount = duk_get_int(ctx, -1);
		duk_pop(ctx);

		duk_peval_string(ctx, "__bitsybox_should_start_tune_demo__");
		shouldStartTuneDemo = duk_get_boolean(ctx, -1);
		duk_pop(ctx);
	}

	duk_destroy_heap(ctx);
}

void gameLoop() {
	duk_context* ctx = duk_create_heap(NULL, NULL, NULL, NULL, fatalError);

	// loop time
	int prevTime = SDL_GetTicks();
	int deltaTime = 0;

	int isGameOver = 0;

	initSystem(ctx);

	shouldContinue = shouldContinue && loadFile(ctx, gameFilePath, "__bitsybox_game_data__");

	duk_peval_string(ctx, "var __bitsybox_is_game_over__ = false;");
	duk_pop(ctx);

	if (gameCount > 1) {
		// hack to return to main menu on game end if there's more than one
		duk_peval_string(ctx, "reset_cur_game = function() { __bitsybox_is_game_over__ = true; };");
		duk_pop(ctx);
	}

	while (shouldContinue && !isGameOver) {
		// keep track of time
		deltaTime = SDL_GetTicks() - prevTime;
		prevTime = SDL_GetTicks();

		updateSystem(ctx, deltaTime);

		// kind of hacky way to trigger restart
		if (duk_peval_string(ctx, "if (bitsy.button(bitsy.BTN_MENU)) { reset_cur_game(); }") != 0) {
			printf("Test Restart Game Error: %s\n", duk_safe_to_string(ctx, -1));
		}
		duk_pop(ctx);

		if (duk_peval_string(ctx, "__bitsybox_is_game_over__") != 0) {
			printf("Test Game Over Error: %s\n", duk_safe_to_string(ctx, -1));
		}
		isGameOver = duk_get_boolean(ctx, -1);
		duk_pop(ctx);
	}

	duk_destroy_heap(ctx);
}

void demoLoop() {
	SDL_SetWindowTitle(window, "DEMO");

	duk_context* ctx = duk_create_heap_default();

	// loop time
	int prevTime = SDL_GetTicks();
	int deltaTime = 0;

	int isTestFinished = 0;

	initSystem(ctx);

	// load demo program
#ifdef BUILD_DEBUG
	shouldContinue = shouldContinue && loadScript(ctx, "test/demo.js");
#else
	// not currently embedding the demo program since it's just for test purposes
	shouldContinue = 0;
#endif

	while (shouldContinue && !isTestFinished) {
		// update time
		deltaTime = SDL_GetTicks() - prevTime;
		prevTime = SDL_GetTicks();

		updateSystem(ctx, deltaTime);
	}

	duk_destroy_heap(ctx);
}

void tuneTool() {
	SDL_SetWindowTitle(window, "TUNE DEMO");

	duk_context* ctx = duk_create_heap_default();

	// loop time
	int prevTime = SDL_GetTicks();
	int deltaTime = 0;

	initSystem(ctx);

	// load tune tool program
#ifdef BUILD_DEBUG
	shouldContinue = shouldContinue && loadScript(ctx, "tune/dialog_demo.js");
	shouldContinue = shouldContinue && loadScript(ctx, "tune/tool_demo.js");
	shouldContinue = shouldContinue && loadScript(ctx, "tune/tune.js");
	shouldContinue = shouldContinue && loadFile(ctx, "tune/tune.bitsy", "__bitsybox_game_data__");
#else
	shouldContinue = shouldContinue && loadEmbeddedScript(ctx, dialog_demo_js);
	shouldContinue = shouldContinue && loadEmbeddedScript(ctx, tool_demo_js);
	shouldContinue = shouldContinue && loadEmbeddedScript(ctx, tune_js);
	shouldContinue = shouldContinue && loadEmbeddedFile(ctx, tune_bitsy, "__bitsybox_game_data__");
#endif

	int shouldQuitToolDemo = 0;

	while (shouldContinue && !shouldQuitToolDemo) {
		// update time
		deltaTime = SDL_GetTicks() - prevTime;
		prevTime = SDL_GetTicks();

		// update mouse state
		int isMouseOnScreen = (mouseX >= 0 && mouseX < BITSY_VIDEO_SIZE && mouseY >= 0 && mouseY < BITSY_VIDEO_SIZE);
		duk_push_boolean(ctx, isLeftMouseDown && isMouseOnScreen);
		duk_put_global_string(ctx, "__bitsybox_mouse_down__");

		duk_push_int(ctx, mouseX);
		duk_put_global_string(ctx, "__bitsybox_mouse_x__");

		duk_push_int(ctx, mouseY);
		duk_put_global_string(ctx, "__bitsybox_mouse_y__");

		duk_push_boolean(ctx, (isButtonLAlt || isButtonRAlt));
		duk_put_global_string(ctx, "__bitsybox_mouse_alt__");

		duk_push_boolean(ctx, isMouseOnScreen);
		duk_put_global_string(ctx, "__bitsybox_mouse_hover__");

		updateSystem(ctx, deltaTime);

		// check if it's time to quit
		duk_peval_string(ctx, "__bitsybox_should_quit_tool_demo__");
		shouldQuitToolDemo = duk_get_boolean(ctx, -1);
		duk_pop(ctx);
	}

	duk_destroy_heap(ctx);
}

/* # BITSYBOX MAIN */

int main(int argc, char* argv[]) {
	printf("~*~*~ bitsybox ~*~*~\n");
	printf("[duktape version 2.6.0]\n");

	// initialize graphics settings
	curGraphicsMode = BITSY_GFX_VIDEO;
	curTextMode = BITSY_TXT_HIREZ;

	// initialize all palette colors to black
	for (int i = 0; i < PALETTE_MAX; i++) {
		systemPalette[i] = (Color) { 0, 0, 0 };
	}

	// initialize audio settings
	dutyChannel1 = BITSY_PULSE_1_2;
	dutyChannel2 = BITSY_PULSE_1_2;
	audioVolume = 0.5f;
	soundChannel1 = wave(440.0f, 1.0f / 2.0f); // A4 square wave
	soundChannel2 = wave(440.0f, 1.0f / 2.0f);

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) < 0) {
		printf("SDL initialization error: %s\n", SDL_GetError());
		return 1;
	}

	// initialize audio
	SDL_AudioSpec audioSpec = {
		.format = AUDIO_F32,
		.channels = 2,
		.freq = AUDIO_SAMPLE_RATE,
		.samples = AUDIO_BUFFER_SIZE,
		.callback = audioCallback,
	};

	if (SDL_OpenAudio(&audioSpec, NULL) < 0) {
		printf("SDL audio error: %s\n", SDL_GetError());
		return 1;
	}

	// start playing audio if initialization succeeded
	SDL_PauseAudio(0);

	// create window
	window = SDL_CreateWindow(
		"BITSYBOX",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		(BITSY_VIDEO_SIZE * renderScale),
		(BITSY_VIDEO_SIZE * renderScale),
		isFullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_RESIZABLE);

	SDL_ShowCursor(!isFullscreen);

	// initialize window settings
	didWindowResizeThisFrame = isFullscreen;
	windowWidth = (BITSY_VIDEO_SIZE * renderScale);
	windowHeight = (BITSY_VIDEO_SIZE * renderScale);

	// create renderer
	renderer = SDL_CreateRenderer(window, -1, 0);

	// set the working directory to the directory containing the executable
	chdir(SDL_GetBasePath());

#if defined(DEMO_MODE)
	while (shouldContinue) {
		demoLoop();
	}
#elif defined(TUNE_TOOL_MODE)
	while (shouldContinue) {
		tuneTool();
	}
#else
	while (shouldContinue) {
		bootMenu();

		if (shouldStartTuneDemo) {
			tuneTool();
		}
		else {
			gameLoop();
		}
	}
#endif

	SDL_Quit();

	return 0;
}