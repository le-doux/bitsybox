#include <stdio.h>
#include <math.h>
#include <string.h>
#include <unistd.h> // for getcwd, chdir
#include <limits.h> // for PATH_MAX
#include <dirent.h>
#include "duktape/duktape.h"

#ifdef PLATFORM_WIN
#include "SDL.h"
#else
#include <SDL2/SDL.h>
#endif

#ifndef BUILD_DEBUG
#include "engine.h"
#include "font.h"
#include "boot.h"
#endif

#define SYSTEM_PALETTE_MAX 256
#define SYSTEM_DRAWING_BUFFER_MAX 1024

/* GAME SELECT */
char gameFilePath[256];
int gameCount = 0;

/* WINDOW */
#if defined(PLATFORM_RPI) && !defined(BUILD_DEBUG)
int isFullscreen = 1;
#else
int isFullscreen = 0;
#endif
int isFullscreenShortcut = 0;
int didWindowResizeThisFrame = 0;

/* SDL */
SDL_Window* window;
SDL_Event event;
SDL_Renderer* renderer;

/* GLOBALS */
int screenSize = 128;
int tileSize = 8;
int roomSize = 16;
int renderScale = 4;
int textboxRenderScale = 2;

int shouldContinue = 1;

/* GRAPHICS */
typedef struct Color {
	int r;
	int g;
	int b;
} Color;

int curGraphicsMode = 0;
Color systemPalette[SYSTEM_PALETTE_MAX];
int curBufferId = -1;
SDL_Texture* drawingBuffers[SYSTEM_DRAWING_BUFFER_MAX];

int screenBufferId = 0;
int textboxBufferId = 1;
int tileStartBufferId = 2;
int nextBufferId = 2;

int textboxWidth = 0;
int textboxHeight = 0;

int windowWidth = 0;
int windowHeight = 0;
SDL_Rect screenBufferRect = { 0, 0, 0, 0 };

/* INPUT */
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
int isButtonPadUp = 0;
int isButtonPadDown = 0;
int isButtonPadLeft = 0;
int isButtonPadRight = 0;
int isButtonPadA = 0;
int isButtonPadB = 0;
int isButtonPadX = 0;
int isButtonPadY = 0;
int isButtonPadStart = 0;

/* FILE LOADING */
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

	// todo: use duk_compile instead?

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

/* Bitsy System APIs */
duk_ret_t bitsyLog(duk_context* ctx) {
	const char* printStr;
	printStr = duk_safe_to_string(ctx, 0);

	printf("bitsy::%s\n", printStr);

	// todo : category parameter

	return 0;
}

duk_ret_t bitsyGetButton(duk_context* ctx) {
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

duk_ret_t bitsySetGraphicsMode(duk_context* ctx) {
	curGraphicsMode = duk_get_int(ctx, 0);

	return 0;
}

duk_ret_t bitsySetColor(duk_context* ctx) {
	int paletteIndex = duk_get_int(ctx, 0);
	int r = duk_get_int(ctx, 1);
	int g = duk_get_int(ctx, 2);
	int b = duk_get_int(ctx, 3);

	systemPalette[paletteIndex] = (Color) { r, g, b };

	return 0;
}

duk_ret_t bitsyResetColors(duk_context* ctx) {
	for (int i = 0; i < SYSTEM_PALETTE_MAX; i++) {
		systemPalette[i] = (Color) { 0, 0, 0 };
	}

	return 0;
}

duk_ret_t bitsyDrawBegin(duk_context* ctx) {
	curBufferId = duk_get_int(ctx, 0);

	// it's faster if we don't switch render targets constantly
	SDL_SetRenderTarget(renderer, drawingBuffers[curBufferId]);

	return 0;
}

duk_ret_t bitsyDrawEnd(duk_context* ctx) {
	curBufferId = -1;

	SDL_SetRenderTarget(renderer, NULL);

	return 0;
}

duk_ret_t bitsyDrawPixel(duk_context* ctx) {
	int paletteIndex = duk_get_int(ctx, 0);
	int x = duk_get_int(ctx, 1);
	int y = duk_get_int(ctx, 2);

	Color color = systemPalette[paletteIndex];

	// printf("draw pixel %d - %d - %d %d %d - %d %d\n", curBufferId, paletteIndex, color.r, color.g, color.b, x, y);

	if (curBufferId == 0 && curGraphicsMode == 0) {
		SDL_Rect pixelRect = { (x * renderScale), (y * renderScale), renderScale, renderScale, };
		SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 0x00);
		SDL_RenderFillRect(renderer, &pixelRect);
	}
	else if (curBufferId == 1 && curGraphicsMode == 1) {
		SDL_Rect pixelRect = { (x * textboxRenderScale), (y * textboxRenderScale), textboxRenderScale, textboxRenderScale, };
		SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 0x00);
		SDL_RenderFillRect(renderer, &pixelRect);
	}
	else if (curBufferId >= tileStartBufferId && curBufferId < nextBufferId && curGraphicsMode == 1) {
		SDL_Rect pixelRect = { (x * renderScale), (y * renderScale), renderScale, renderScale, };
		SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 0x00);
		SDL_RenderFillRect(renderer, &pixelRect);
	}

	return 0;
}

duk_ret_t bitsyDrawTile(duk_context* ctx) {
	// can only draw tiles on the screen buffer in tile mode
	if (curBufferId != 0 || curGraphicsMode != 1) {
		return 0;
	}

	int tileId = duk_get_int(ctx, 0);
	int x = duk_get_int(ctx, 1);
	int y = duk_get_int(ctx, 2);

	if (tileId < tileStartBufferId || tileId >= nextBufferId) {
		return 0;
	}

	SDL_Rect tileRect = {
		(x * tileSize * renderScale),
		(y * tileSize * renderScale),
		(tileSize * renderScale),
		(tileSize * renderScale),
	};

	SDL_RenderCopy(renderer, drawingBuffers[tileId], NULL, &tileRect);

	return 0;
}

duk_ret_t bitsyDrawTextbox(duk_context* ctx) {
	// can only draw the textbox on the screen buffer in tile mode
	if (curBufferId != 0 || curGraphicsMode != 1) {
		return 0;
	}

	int x = duk_get_int(ctx, 0);
	int y = duk_get_int(ctx, 1);

	SDL_Rect textboxRect = {
		(x * renderScale),
		(y * renderScale),
		(textboxWidth * textboxRenderScale),
		(textboxHeight * textboxRenderScale),
	};

	SDL_RenderCopy(renderer, drawingBuffers[1], NULL, &textboxRect);

	return 0;
}

duk_ret_t bitsyClear(duk_context* ctx) {
	int paletteIndex = duk_get_int(ctx, 0);

	Color color = systemPalette[paletteIndex];

	if (curBufferId == 0) {
		SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 0x00);
		SDL_Rect fillRect = { 0, 0, (screenSize * renderScale), (screenSize * renderScale), };
		SDL_RenderFillRect(renderer, &fillRect);
	}
	else if (curBufferId == 1) {
		SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 0x00);
		SDL_Rect fillRect = { 0, 0, (textboxWidth * textboxRenderScale), (textboxHeight * textboxRenderScale), };
		SDL_RenderFillRect(renderer, &fillRect);
	}
	else if (curBufferId >= tileStartBufferId && curBufferId < nextBufferId) {
		SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 0x00);
		SDL_Rect fillRect = { 0, 0, (tileSize * renderScale), (tileSize * renderScale), };
		SDL_RenderFillRect(renderer, &fillRect);
	}

	return 0;
}

duk_ret_t bitsyAddTile(duk_context* ctx) {
	if (nextBufferId >= SYSTEM_DRAWING_BUFFER_MAX) {
		// todo : error handling?
		return 0;
	}

	drawingBuffers[nextBufferId] = SDL_CreateTexture(
		renderer,
		SDL_PIXELFORMAT_RGB888,
		SDL_TEXTUREACCESS_TARGET,
		(tileSize * renderScale),
		(tileSize * renderScale));

	duk_push_int(ctx, nextBufferId);

	nextBufferId++;

	return 1;
}

duk_ret_t bitsyResetTiles(duk_context* ctx) {
	nextBufferId = tileStartBufferId;

	return 0;
}

duk_ret_t bitsySetTextboxSize(duk_context* ctx) {
	textboxWidth = duk_get_int(ctx, 0);
	textboxHeight = duk_get_int(ctx, 1);

	drawingBuffers[1] = SDL_CreateTexture(
		renderer,
		SDL_PIXELFORMAT_RGB888,
		SDL_TEXTUREACCESS_TARGET,
		(textboxWidth * textboxRenderScale),
		(textboxHeight * textboxRenderScale));

	return 0;
}

duk_ret_t bitsyOnLoad(duk_context* ctx) {
	// hacky to just stick it in the global namespace??
	duk_put_global_string(ctx, "__bitsybox_on_load__");

	return 0;
}

duk_ret_t bitsyOnQuit(duk_context* ctx) {
	duk_put_global_string(ctx, "__bitsybox_on_quit__");

	return 0;
}

duk_ret_t bitsyOnUpdate(duk_context* ctx) {
	duk_put_global_string(ctx, "__bitsybox_on_update__");

	return 0;
}

static void fatalError(void* udata, const char* msg) {
	(void) udata; // not used currently
    fprintf(stderr, "*** FATAL ERROR: %s\n", (msg ? msg : "no message"));
    fflush(stderr);
    abort();
}

void initBitsySystem(duk_context* ctx) {
	duk_push_c_function(ctx, bitsyLog, 2);
	duk_put_global_string(ctx, "bitsyLog");

	duk_push_c_function(ctx, bitsyGetButton, 1);
	duk_put_global_string(ctx, "bitsyGetButton");

	duk_push_c_function(ctx, bitsySetGraphicsMode, 1);
	duk_put_global_string(ctx, "bitsySetGraphicsMode");

	duk_push_c_function(ctx, bitsySetColor, 4);
	duk_put_global_string(ctx, "bitsySetColor");

	duk_push_c_function(ctx, bitsyResetColors, 0);
	duk_put_global_string(ctx, "bitsyResetColors");

	duk_push_c_function(ctx, bitsyDrawBegin, 1);
	duk_put_global_string(ctx, "bitsyDrawBegin");

	duk_push_c_function(ctx, bitsyDrawEnd, 0);
	duk_put_global_string(ctx, "bitsyDrawEnd");

	duk_push_c_function(ctx, bitsyDrawPixel, 3);
	duk_put_global_string(ctx, "bitsyDrawPixel");

	duk_push_c_function(ctx, bitsyDrawTile, 3);
	duk_put_global_string(ctx, "bitsyDrawTile");

	duk_push_c_function(ctx, bitsyDrawTextbox, 2);
	duk_put_global_string(ctx, "bitsyDrawTextbox");

	duk_push_c_function(ctx, bitsyClear, 1);
	duk_put_global_string(ctx, "bitsyClear");

	duk_push_c_function(ctx, bitsyAddTile, 0);
	duk_put_global_string(ctx, "bitsyAddTile");

	duk_push_c_function(ctx, bitsyResetTiles, 0);
	duk_put_global_string(ctx, "bitsyResetTiles");

	duk_push_c_function(ctx, bitsySetTextboxSize, 2);
	duk_put_global_string(ctx, "bitsySetTextboxSize");

	duk_push_c_function(ctx, bitsyOnLoad, 1);
	duk_put_global_string(ctx, "bitsyOnLoad");

	duk_push_c_function(ctx, bitsyOnQuit, 1);
	duk_put_global_string(ctx, "bitsyOnQuit");

	duk_push_c_function(ctx, bitsyOnUpdate, 1);
	duk_put_global_string(ctx, "bitsyOnUpdate");
}

void loadEngine(duk_context* ctx) {
#ifdef BUILD_DEBUG
	// load engine
	shouldContinue = shouldContinue && loadScript(ctx, "bitsy/engine/script.js");
	shouldContinue = shouldContinue && loadScript(ctx, "bitsy/engine/font.js");
	shouldContinue = shouldContinue && loadScript(ctx, "bitsy/engine/transition.js");
	shouldContinue = shouldContinue && loadScript(ctx, "bitsy/engine/dialog.js");
	shouldContinue = shouldContinue && loadScript(ctx, "bitsy/engine/renderer.js");
	shouldContinue = shouldContinue && loadScript(ctx, "bitsy/engine/bitsy.js");
	// load default font
	shouldContinue = shouldContinue && loadFile(ctx, "bitsy/font/ascii_small.bitsyfont", "__bitsybox_default_font__");
#else
	// load engine
	shouldContinue = shouldContinue && loadEmbeddedScript(ctx, script_js);
	shouldContinue = shouldContinue && loadEmbeddedScript(ctx, font_js);
	shouldContinue = shouldContinue && loadEmbeddedScript(ctx, transition_js);
	shouldContinue = shouldContinue && loadEmbeddedScript(ctx, dialog_js);
	shouldContinue = shouldContinue && loadEmbeddedScript(ctx, renderer_js);
	shouldContinue = shouldContinue && loadEmbeddedScript(ctx, bitsy_js);
	// load default font
	shouldContinue = shouldContinue && loadEmbeddedFile(ctx, ascii_small_bitsyfont, "__bitsybox_default_font__");
#endif
}

void updateInput() {
	if (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT) {
			shouldContinue = 0; // todo : use break instead?
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
		else if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
			// todo ??
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

	// toggle fullscreen
	int prevFrameFullscreenShortcut = isFullscreenShortcut;
	isFullscreenShortcut = (isButtonLAlt || isButtonRAlt) && isButtonReturn;

	if (isFullscreenShortcut && !prevFrameFullscreenShortcut) {
		isFullscreen = (isFullscreen == 0 ? 1 : 0);

		SDL_ShowCursor(!isFullscreen);
		SDL_SetWindowFullscreen(window, isFullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);

		if (!isFullscreen) {
			SDL_SetWindowSize(window, (screenSize * renderScale), (screenSize * renderScale));
		}

		didWindowResizeThisFrame = 1;
	}
}

void onWindowResize(duk_context* ctx) {
	SDL_GetWindowSize(window, &windowWidth, &windowHeight);

	int size = (windowWidth <= windowHeight) ? windowWidth : windowHeight;

	screenBufferRect = (SDL_Rect) {
		(windowWidth / 2) - (size / 2),
		(windowHeight / 2) - (size / 2),
		size,
		size,
	};

	// hacky way to force a re-render of all the tiles, since SDL2 needs that when the window is resized
	duk_peval_string(ctx, "renderer.ClearCache();");
	duk_pop(ctx);
}

void bootMenu() {
	SDL_SetWindowTitle(window, "BITSYBOX");

	duk_context* ctx = duk_create_heap_default();

	initBitsySystem(ctx);

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
	int loopTime = 0;
	int loopTimeMax = 16;

	int isBootFinished = 0;

	loadEngine(ctx);

	// load boot menu
#ifdef BUILD_DEBUG
	shouldContinue = shouldContinue && loadScript(ctx, "boot/boot.js");
	shouldContinue = shouldContinue && loadFile(ctx, "boot/boot.bitsy", "__bitsybox_game_data__");
#else
	shouldContinue = shouldContinue && loadEmbeddedScript(ctx, boot_js);
	shouldContinue = shouldContinue && loadEmbeddedFile(ctx, boot_bitsy, "__bitsybox_game_data__");
#endif

	if (duk_peval_string(ctx, "__bitsybox_on_load__(__bitsybox_game_data__, __bitsybox_default_font__);") != 0) {
		printf("Load Boot Menu Error: %s\n", duk_safe_to_string(ctx, -1));
	}
	duk_pop(ctx);

	while (shouldContinue && !isBootFinished) {
		// update time
		deltaTime = SDL_GetTicks() - prevTime;
		prevTime = SDL_GetTicks();
		loopTime += deltaTime;

		updateInput();

		if (loopTime >= loopTimeMax && shouldContinue) {
			Color bg = systemPalette[0];
			SDL_SetRenderTarget(renderer, NULL);
			SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, 0x00);
			SDL_RenderClear(renderer);

			// main loop
			if (duk_peval_string(ctx, "__bitsybox_on_update__();") != 0) {
				printf("Update Boot Menu Error: %s\n", duk_safe_to_string(ctx, -1));
			}
			duk_pop(ctx);

			// copy the screen buffer texture into the renderer
			SDL_SetRenderTarget(renderer, NULL);
			SDL_RenderCopy(renderer, drawingBuffers[0], NULL, &screenBufferRect);

			// show the frame
			SDL_RenderPresent(renderer);

			loopTime = 0;

			duk_peval_string(ctx, "__bitsybox_is_boot_finished__");
			isBootFinished = duk_get_boolean(ctx, -1);
			duk_pop(ctx);
		}

		if (didWindowResizeThisFrame) {
			onWindowResize(ctx);
		}

		didWindowResizeThisFrame = 0;
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
	}

	if (duk_peval_string(ctx, "__bitsybox_on_quit__();") != 0) {
		printf("Quit Boot Menu Error: %s\n", duk_safe_to_string(ctx, -1));
	}
	duk_pop(ctx);

	duk_destroy_heap(ctx);
}

void gameLoop() {
	duk_context* ctx = duk_create_heap(NULL, NULL, NULL, NULL, fatalError);

	initBitsySystem(ctx);

	// loop time
	int prevTime = SDL_GetTicks();
	int deltaTime = 0;
	int loopTime = 0;
	int loopTimeMax = 16;

	int isGameOver = 0;

	loadEngine(ctx);

	shouldContinue = shouldContinue && loadFile(ctx, gameFilePath, "__bitsybox_game_data__");

	duk_peval_string(ctx, "var __bitsybox_is_game_over__ = false;");
	duk_pop(ctx);

	// main loop
	if (duk_peval_string(ctx, "__bitsybox_on_load__(__bitsybox_game_data__, __bitsybox_default_font__);") != 0) {
		printf("Load Bitsy Error: %s\n", duk_safe_to_string(ctx, -1));
	}
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
		loopTime += deltaTime;

		// printf("dt %d\n", deltaTime); // debug frame rate

		updateInput();

		if (loopTime >= loopTimeMax && shouldContinue) {
			Color bg = systemPalette[0];
			SDL_SetRenderTarget(renderer, NULL);
			SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, 0x00);
			SDL_RenderClear(renderer);

			// main loop
			if (duk_peval_string(ctx, "__bitsybox_on_update__();") != 0) {
				printf("Update Bitsy Error: %s\n", duk_safe_to_string(ctx, -1));
			}
			duk_pop(ctx);

			// copy the screen buffer texture into the renderer
			SDL_SetRenderTarget(renderer, NULL);
			SDL_RenderCopy(renderer, drawingBuffers[0], NULL, &screenBufferRect);

			// show the frame
			SDL_RenderPresent(renderer);

			loopTime = 0;
		}

		// kind of hacky way to trigger restart
		if (duk_peval_string(ctx, "if (bitsyGetButton(5)) { reset_cur_game(); }") != 0) {
			printf("Test Restart Game Error: %s\n", duk_safe_to_string(ctx, -1));
		}
		duk_pop(ctx);

		if (duk_peval_string(ctx, "__bitsybox_is_game_over__") != 0) {
			printf("Test Game Over Error: %s\n", duk_safe_to_string(ctx, -1));
		}
		isGameOver = duk_get_boolean(ctx, -1);
		duk_pop(ctx);

		if (didWindowResizeThisFrame) {
			onWindowResize(ctx);
		}

		didWindowResizeThisFrame = 0;
	}

	if (duk_peval_string(ctx, "__bitsybox_on_quit__();") != 0) {
		printf("Quit Bitsy Error: %s\n", duk_safe_to_string(ctx, -1));
	}
	duk_pop(ctx);

	duk_destroy_heap(ctx);
}

int main(int argc, char* argv[]) {
	printf("~*~*~ bitsybox ~*~*~\n");
	printf("[duktape version 2.6.0]\n");

	// set the working directory to the directory containing the executable
	chdir(SDL_GetBasePath());

	// debug pal
	systemPalette[0] = (Color) { 255, 0, 0};
	systemPalette[1] = (Color) { 0, 255, 0};
	systemPalette[2] = (Color) { 0, 0, 255};

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER);

	window = SDL_CreateWindow(
		"BITSYBOX",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		(screenSize * renderScale),
		(screenSize * renderScale),
		isFullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_RESIZABLE);

	SDL_ShowCursor(!isFullscreen);
	didWindowResizeThisFrame = isFullscreen;

	renderer = SDL_CreateRenderer(window, -1, 0);

	// create screen texture
	drawingBuffers[0] = SDL_CreateTexture(
		renderer,
		SDL_PIXELFORMAT_RGB888,
		SDL_TEXTUREACCESS_TARGET,
		(screenSize * renderScale),
		(screenSize * renderScale));

	windowWidth = (screenSize * renderScale);
	windowHeight = (screenSize * renderScale);
	screenBufferRect = (SDL_Rect) { 0, 0, (screenSize * renderScale), (screenSize * renderScale) };

	// create textbox texture
	drawingBuffers[1] = SDL_CreateTexture(
		renderer,
		SDL_PIXELFORMAT_RGB888,
		SDL_TEXTUREACCESS_TARGET,
		(textboxWidth * textboxRenderScale),
		(textboxHeight * textboxRenderScale));

	while (shouldContinue) {
		bootMenu();
		gameLoop();
	}

	SDL_Quit();

	return 0;
}
