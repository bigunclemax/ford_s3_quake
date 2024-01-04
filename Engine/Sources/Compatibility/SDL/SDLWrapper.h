#ifndef SDLWrapper_h
#define SDLWrapper_h

#include <screen/screen.h>

#include <stdint.h>
#include <stdbool.h>

#define SDL_WINDOW_FULLSCREEN 1
#define SDL_INIT_VIDEO 1

typedef struct {
    bool exitRequested;
	screen_context_t screen_ctx;
	screen_window_t screen_win;
	bool winInitialized;
	int windowWidth;
	int windowHeight;
} SdlwContext;

extern SdlwContext *sdlwContext;

bool sdlwInitialize();
void sdlwFinalize();

bool sdlwCreateWindow();
void sdlwDestroyWindow();

bool sdlwIsExitRequested();
void sdlwRequestExit(bool flag);
bool sdlwResize(int w, int h);

void sdlwEnableDefaultEventManagement(bool flag);
void sdlwCheckEvents();

bool sdlwGetSize(int *w, int *h);

#endif
