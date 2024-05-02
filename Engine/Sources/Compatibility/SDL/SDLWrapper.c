#include "SDL/SDLWrapper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/common.h"

#define R_ZORDER_DEFAULT_STRING "2147483647" //INT_MAX
cvar_t *r_window_zorder;

SdlwContext *sdlwContext = NULL;

//SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK
#if 0
static void sdlwLogOutputFunction(void *userdata, int category, SDL_LogPriority priority, const char *message)
{
    const char *categoryString = "";
    switch (category)
    {
    default:
        categoryString = "unknown";
        break;
    case SDL_LOG_CATEGORY_APPLICATION:
        categoryString = "application";
        break;
    case SDL_LOG_CATEGORY_ERROR:
        categoryString = "error";
        break;
    case SDL_LOG_CATEGORY_ASSERT:
        categoryString = "assert";
        break;
    case SDL_LOG_CATEGORY_SYSTEM:
        categoryString = "system";
        break;
    case SDL_LOG_CATEGORY_AUDIO:
        categoryString = "audio";
        break;
    case SDL_LOG_CATEGORY_VIDEO:
        categoryString = "video";
        break;
    case SDL_LOG_CATEGORY_RENDER:
        categoryString = "render";
        break;
    case SDL_LOG_CATEGORY_INPUT:
        categoryString = "input";
        break;
    case SDL_LOG_CATEGORY_TEST:
        categoryString = "test";
        break;
    }

    const char *priorityString = "unknown";
    switch (priority)
    {
    default:
        priorityString = "unknown";
        break;
    case SDL_LOG_PRIORITY_VERBOSE:
        priorityString = "verbose";
        break;
    case SDL_LOG_PRIORITY_DEBUG:
        priorityString = "debug";
        break;
    case SDL_LOG_PRIORITY_INFO:
        priorityString = "info";
        break;
    case SDL_LOG_PRIORITY_WARN:
        priorityString = "warn";
        break;
    case SDL_LOG_PRIORITY_ERROR:
        priorityString = "error";
        break;
    case SDL_LOG_PRIORITY_CRITICAL:
        priorityString = "critical";
        break;
    }
    
    printf("SDL - %s - %s - %s", categoryString, priorityString, message);
}

bool sdlwInitialize(SdlProcessEventFunction processEvent, Uint32 flags) {
#endif

bool sdlwInitialize() {
    int rc;
    sdlwFinalize();
    
	SdlwContext *sdlw = malloc(sizeof(SdlwContext));
	if (sdlw == NULL) return true;
	sdlwContext = sdlw;
    sdlw->exitRequested = false;
#if 0
    sdlw->defaultEventManagementEnabled = true;
	sdlw->processEvent = processEvent;
    sdlw->window = NULL;
#endif
    sdlw->winInitialized = false;
    sdlw->windowWidth = 0;
    sdlw->windowHeight = 0;
#if 0
    if (SDL_Init(flags) < 0) {
        printf("Unable to initialize SDL: %s\n", SDL_GetError());
        goto on_error;
    }
    
    SDL_LogSetOutputFunction(sdlwLogOutputFunction, NULL);
//    SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Test\n");
    
//    if (SDL_NumJoysticks() > 0) SDL_JoystickOpen(0);
#endif

	rc = screen_create_context(&(sdlw->screen_ctx), SCREEN_WINDOW_MANAGER_CONTEXT);
	if (rc) {
		fprintf(stderr, "iow_create_context failed with error %d (0x%08x)\n", rc, rc);
		goto on_error;
	}

    return false;
on_error:
	sdlwFinalize();
    return true;
}

void sdlwFinalize() {
	SdlwContext *sdlw = sdlwContext;
	if (sdlw == NULL) return;

    screen_destroy_context(sdlw->screen_ctx);

    free(sdlw);
    sdlwContext = NULL;
}

bool sdlwCreateWindow()
{
	int rc;
	int zorder = 0;
	int usage = SCREEN_USAGE_OPENGL_ES2;
	int transp = SCREEN_TRANSPARENCY_NONE;
	int size[2] = { -1, -1 };
	int interval = 1;
	int nbuffers = 2;

	SdlwContext *sdlw = sdlwContext;
	if (sdlw == NULL) return true;

    sdlwDestroyWindow();

	rc = screen_create_window(&sdlw->screen_win, sdlw->screen_ctx);
	if (rc) {
		fprintf(stderr, "screen_create_window\n");
		goto on_error_1;
	}

	r_window_zorder = Cvar_Get("r_window_zorder", R_ZORDER_DEFAULT_STRING, CVAR_ARCHIVE);
	zorder = (int)r_window_zorder->value;

	rc = screen_set_window_property_iv(sdlw->screen_win, SCREEN_PROPERTY_ZORDER, &zorder);
	if (rc) {
		fprintf(stderr, "screen_set_window_property_iv(SCREEN_PROPERTY_ZORDER)\n");
		goto on_error_2;
	}

	fprintf(stderr, "FIXME: handle format (SCREEN_PROPERTY_FORMAT)\n");
#if 0
	format = choose_format(egl_disp, egl_conf);
	rc = screen_set_window_property_iv(sdlw->screen_win, SCREEN_PROPERTY_FORMAT, &format);
	if (rc) {
		fprintf(stderr, "screen_set_window_property_iv(SCREEN_PROPERTY_FORMAT)\n");
		goto on_error_2;
	}
#endif
	rc = screen_set_window_property_iv(sdlw->screen_win, SCREEN_PROPERTY_USAGE, &usage);
	if (rc) {
		fprintf(stderr, "screen_set_window_property_iv\n");
		goto on_error_2;
	}

	//FIXME: suggested windowWidth and windowHeight are ignored
#if 0
	if (size[0] > 0 && size[1] > 0) {
		rc = screen_set_window_property_iv(sdlw->screen_win, SCREEN_PROPERTY_SIZE, size);
		if (rc) {
			fprintf(stderr, "screen_set_window_property_iv(SCREEN_PROPERTY_SIZE)\n");
			goto fail5;
		}
	} else
#endif
    {
		rc = screen_get_window_property_iv(sdlw->screen_win, SCREEN_PROPERTY_SIZE, size);
		if (rc) {
			fprintf(stderr, "screen_get_window_property_iv(SCREEN_PROPERTY_SIZE)\n");
			goto on_error_2;
		}
	}

#if 0
	if (pos[0] != 0 || pos[1] != 0) {
		rc = screen_set_window_property_iv(sdlw->screen_win, SCREEN_PROPERTY_POSITION, pos);
		if (rc) {
			fprintf(stderr, "screen_set_window_property_iv(SCREEN_PROPERTY_POSITION)\n");
			goto fail5;
		}
	}
#endif

	rc = screen_set_window_property_iv(sdlw->screen_win, SCREEN_PROPERTY_TRANSPARENCY, &transp);
	if (rc) {
		fprintf(stderr, "screen_set_window_property_iv(SCREEN_PROPERTY_TRANSPARENCY)\n");
		goto on_error_2;
	}

	rc = screen_set_window_property_iv(sdlw->screen_win, SCREEN_PROPERTY_SWAP_INTERVAL, &interval);
	if (rc) {
		fprintf(stderr, "screen_set_window_property_iv(SCREEN_PROPERTY_SWAP_INTERVAL)\n");
		goto on_error_2;
	}

	// Set ID string for debugging via /dev/screen.
	{
		const char *idstr = "quake2";
		screen_set_window_property_cv(sdlw->screen_win,
				SCREEN_PROPERTY_ID_STRING, strlen(idstr), idstr);
	}

	rc = screen_create_window_buffers(sdlw->screen_win, nbuffers);
	if (rc) {
		fprintf(stderr, "screen_create_window_buffers\n");
		goto on_error_2;
	}

	sdlw->windowWidth = size[0];
	sdlw->windowHeight = size[1];
    sdlw->winInitialized = true;

	fprintf(stderr, "Window created: W: %d  H: %d\n", sdlw->windowWidth, sdlw->windowHeight);

    return false;

on_error_2:
    screen_destroy_window(sdlw->screen_win);
on_error_1:
    return true;
}

void sdlwDestroyWindow() {
	SdlwContext *sdlw = sdlwContext;
	if (sdlw == NULL) return;

    if (sdlw->winInitialized) {
        screen_destroy_window(sdlw->screen_win);
        sdlw->windowWidth = 0;
        sdlw->windowHeight = 0;
    }
}

bool sdlwIsExitRequested()
{
	SdlwContext *sdlw = sdlwContext;
    if (sdlw == NULL) return true;
    return sdlw->exitRequested;
}

void sdlwRequestExit(bool flag)
{
	SdlwContext *sdlw = sdlwContext;
    if (sdlw == NULL) return;
    sdlw->exitRequested = flag;
}

bool sdlwResize(int w, int h) {
	SdlwContext *sdlw = sdlwContext;
	sdlw->windowWidth = w;
	sdlw->windowHeight = h;
    return false;
}

#if 0
void sdlwEnableDefaultEventManagement(bool flag)
{
	SdlwContext *sdlw = sdlwContext;
    if (sdlw == NULL) return;
    sdlw->defaultEventManagementEnabled = flag;
}

static void sdlwManageEvent(SdlwContext *sdlw, SDL_Event *event) {
    switch (event->type) {
    default:
        break;

    case SDL_QUIT:
        printf("Exit requested by the system.");
        sdlwRequestExit(true);
        break;

    case SDL_WINDOWEVENT:
        switch (event->window.event) {
        case SDL_WINDOWEVENT_CLOSE:
            printf("Exit requested by the user (by closing the window).");
            sdlwRequestExit(true);
            break;
        case SDL_WINDOWEVENT_RESIZED:
            sdlwResize(event->window.data1, event->window.data2);
            break;
        }
        break;

    case SDL_KEYDOWN:
        switch (event->key.keysym.sym) {
        default:
            break;
        case 27:
            printf("Exit requested by the user (with a key).");
            sdlwRequestExit(true);
            break;
        }
        break;
    }
}

void sdlwCheckEvents() {
	SdlwContext *sdlw = sdlwContext;
    if (sdlw == NULL) return;
    
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
        bool eventManaged = false;
		SdlProcessEventFunction processEvent = sdlw->processEvent;
		if (processEvent != NULL)
			eventManaged = processEvent(&event);
        if (!eventManaged && sdlw->defaultEventManagementEnabled)
            sdlwManageEvent(sdlw, &event);
	}
}

#endif

bool sdlwGetSize(int *w, int *h) {
	SdlwContext *sdlw = sdlwContext;

    if ((sdlw == NULL) || (!sdlw->winInitialized)) {
        return true;
    }

    *w = sdlw->windowWidth;
    *h = sdlw->windowHeight;

    return false;
}
