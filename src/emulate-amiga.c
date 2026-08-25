#include "rt.h"
#include "exec/exec.h"
#include "intuition/intuition.h"

#ifdef JUGGLER_USE_SDL2
#define SDL_MAIN_HANDLED
#include <SDL.h>
#else
#include <SDL3/SDL.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef JUGGLER_USE_SDL2
#define JUGGLER_SDL_QUIT SDL_QUIT
#define JUGGLER_SDL_KEYDOWN SDL_KEYDOWN
#define JUGGLER_SDL_KEY(event) ((event).key.keysym.sym)
typedef SDL_Keycode JugglerKeycode;
#else
#define JUGGLER_SDL_QUIT SDL_EVENT_QUIT
#define JUGGLER_SDL_KEYDOWN SDL_EVENT_KEY_DOWN
#define JUGGLER_SDL_KEY(event) ((event).key.key)
typedef SDL_Keycode JugglerKeycode;
#endif

struct EmuState {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    unsigned char *ham;
    unsigned char *rgb;
    int width;
    int height;
    int display_width;
    int display_height;
    int palette[32][3];
    int initialized;
    int dirty;
};

static struct EmuState emu;
static struct GfxBase gfxbase;
static struct IntuitionBase intuitionbase;
static struct DosBase dosbase;
static struct RastPort rastport;

static int init_video(void);
static SDL_Renderer *create_renderer(SDL_Window *window);
static void render_texture(SDL_Renderer *renderer,SDL_Texture *texture,SDL_FRect *dst);

static unsigned char dac4(int v)
{
    if (v < 0) v=0;
    if (v > 15) v=15;
    return (unsigned char)(v * 17);
}

static void present(void)
{
    SDL_FRect dst;

    if (!emu.dirty) return;
    if (!emu.renderer || !emu.texture || !emu.rgb) return;
    SDL_UpdateTexture(emu.texture,NULL,emu.rgb,emu.width*3);
    SDL_SetRenderDrawColor(emu.renderer,0,0,0,255);
    SDL_RenderClear(emu.renderer);
    dst.x=0.0f;
    dst.y=0.0f;
    dst.w=(float)emu.display_width;
    dst.h=(float)emu.display_height;
    render_texture(emu.renderer,emu.texture,&dst);
    SDL_RenderPresent(emu.renderer);
    emu.dirty=0;
}

static int closes_preview(JugglerKeycode key)
{
    if (key >= 0x20 && key <= 0x7e) return 1;
    return key == SDLK_ESCAPE || key == SDLK_RETURN || key == SDLK_KP_ENTER;
}

static void pump_events(int *done)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == JUGGLER_SDL_QUIT) {
            *done=1;
        }
        if (event.type == JUGGLER_SDL_KEYDOWN &&
            closes_preview(JUGGLER_SDL_KEY(event))) {
            *done=1;
        }
    }
}

static int init_video(void)
{
#ifdef JUGGLER_USE_SDL2
    return SDL_Init(SDL_INIT_VIDEO) == 0;
#else
    return SDL_Init(SDL_INIT_VIDEO);
#endif
}

static SDL_Renderer *create_renderer(SDL_Window *window)
{
#ifdef JUGGLER_USE_SDL2
    return SDL_CreateRenderer(window,-1,0);
#else
    return SDL_CreateRenderer(window,NULL);
#endif
}

static void render_texture(SDL_Renderer *renderer,SDL_Texture *texture,SDL_FRect *dst)
{
#ifdef JUGGLER_USE_SDL2
    SDL_RenderCopyF(renderer,texture,NULL,dst);
#else
    SDL_RenderTexture(renderer,texture,NULL,dst);
#endif
}

static void release_window_resources(void)
{
    if (emu.texture) SDL_DestroyTexture(emu.texture);
    if (emu.renderer) SDL_DestroyRenderer(emu.renderer);
    if (emu.window) SDL_DestroyWindow(emu.window);
    free(emu.ham);
    free(emu.rgb);
    emu.texture=NULL;
    emu.renderer=NULL;
    emu.window=NULL;
    emu.ham=NULL;
    emu.rgb=NULL;
}

void *OpenLibrary(const char *name,long version)
{
    (void)version;
    if (name && strcmp(name,"graphics.library") == 0) return &gfxbase;
    if (name && strcmp(name,"intuition.library") == 0) return &intuitionbase;
    if (name && strcmp(name,"dos.library") == 0) return &dosbase;
    return &gfxbase;
}

void CloseLibrary(void *library)
{
    (void)library;
}

struct IOStdReq *CreateStdIO(struct Port *port)
{
    (void)port;
    return NULL;
}

struct Port *CreatePort(const char *name,long pri)
{
    (void)name;
    (void)pri;
    return NULL;
}

struct Screen *OpenScreen(struct NewScreen *newscreen)
{
    struct Screen *screen;

    if (!emu.initialized) {
        if (!init_video()) {
            fprintf(stderr,"SDL_Init failed: %s\n",SDL_GetError());
            return NULL;
        }
        emu.initialized=1;
    }

    screen=(struct Screen *)calloc(1,sizeof(struct Screen));
    if (!screen) return NULL;
    screen->Width=newscreen->Width;
    screen->Height=newscreen->Height;
    screen->Depth=newscreen->Depth;
    return screen;
}

void CloseScreen(struct Screen *screen)
{
    free(screen);
}

struct Window *OpenWindow(struct NewWindow *newwindow)
{
    struct Window *window;
    int scale;

    emu.width=newwindow->Width;
    emu.height=newwindow->Height;
    scale=emu.width < 320 ? 4 : 2;
    emu.display_width=emu.width*scale;
    emu.display_height=(emu.height*scale*6+2)/5;

#ifdef JUGGLER_USE_SDL2
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"0");
#endif

    emu.window=SDL_CreateWindow("raytracer",emu.display_width,emu.display_height,0);
    if (!emu.window) {
        fprintf(stderr,"SDL_CreateWindow failed: %s\n",SDL_GetError());
        return NULL;
    }

    emu.renderer=create_renderer(emu.window);
    if (!emu.renderer) {
        fprintf(stderr,"SDL_CreateRenderer failed: %s\n",SDL_GetError());
        release_window_resources();
        return NULL;
    }

    emu.texture=SDL_CreateTexture(emu.renderer,SDL_PIXELFORMAT_RGB24,SDL_TEXTUREACCESS_STREAMING,emu.width,emu.height);
    if (!emu.texture) {
        fprintf(stderr,"SDL_CreateTexture failed: %s\n",SDL_GetError());
        release_window_resources();
        return NULL;
    }
    SDL_SetTextureScaleMode(emu.texture,SDL_SCALEMODE_NEAREST);

    emu.ham=(unsigned char *)calloc((size_t)emu.width*(size_t)emu.height,1);
    emu.rgb=(unsigned char *)calloc((size_t)emu.width*(size_t)emu.height*3,1);
    if (!emu.ham || !emu.rgb) {
        release_window_resources();
        return NULL;
    }

    window=(struct Window *)calloc(1,sizeof(struct Window));
    if (!window) {
        release_window_resources();
        return NULL;
    }
    window->Screen=newwindow->Screen;
    window->Width=newwindow->Width;
    window->Height=newwindow->Height;
    window->RPort=&rastport;
    rastport.current_pen=0;
    return window;
}

void CloseWindow(struct Window *window)
{
    int done;

    present();
    write_captured_png("image.png");
    done=0;
    while (!done) {
        pump_events(&done);
        SDL_Delay(16);
    }

    free(window);
    release_window_resources();
    memset(&emu,0,sizeof(emu));
    SDL_Quit();
}

struct ViewPort *ViewPortAddress(struct Window *window)
{
    return window ? &window->Screen->ViewPort : NULL;
}

void SetPointer(struct Window *window,UWORD *pointer,short height,short width,short xoffset,short yoffset)
{
    (void)window;
    (void)pointer;
    (void)height;
    (void)width;
    (void)xoffset;
    (void)yoffset;
}

void SetRGB4(struct ViewPort *viewport,short index,short red,short green,short blue)
{
    (void)viewport;
    if (index < 0 || index >= 32) return;
    emu.palette[index][0]=red;
    emu.palette[index][1]=green;
    emu.palette[index][2]=blue;
}

void SetAPen(struct RastPort *rp,short pen)
{
    if (rp) rp->current_pen=pen & 0x3f;
}

void WritePixel(struct RastPort *rp,short x,short y)
{
    int pen,mode,value;
    unsigned char r,g,b;
    size_t offset;
    int done;

    if (!rp || !emu.rgb || !emu.ham) return;
    if (x < 0 || y < 0 || x >= emu.width || y >= emu.height) {
        return;
    }

    pen=rp->current_pen & 0x3f;
    mode=pen & 0x30;
    value=pen & 0x0f;
    offset=((size_t)y*(size_t)emu.width+(size_t)x)*3;

    if (x > 0) {
        size_t prev=offset-3;
        r=emu.rgb[prev+0];
        g=emu.rgb[prev+1];
        b=emu.rgb[prev+2];
    } else {
        r=g=b=0;
    }

    if (mode == 0x00) {
        r=dac4(emu.palette[value][0]);
        g=dac4(emu.palette[value][1]);
        b=dac4(emu.palette[value][2]);
    } else if (mode == 0x10) {
        b=dac4(value);
    } else if (mode == 0x20) {
        r=dac4(value);
    } else {
        g=dac4(value);
    }

    emu.ham[(size_t)y*(size_t)emu.width+(size_t)x]=(unsigned char)pen;
    emu.rgb[offset+0]=r;
    emu.rgb[offset+1]=g;
    emu.rgb[offset+2]=b;
    emu.dirty=1;

    if (x == emu.width-1) {
        done=0;
        pump_events(&done);
        present();
    }
}
