#ifndef INTUITION_INTUITION_H
#define INTUITION_INTUITION_H

#include "exec/types.h"

#define HAM          0x0800
#define CUSTOMSCREEN 0x000F
#define BORDERLESS   0x0800
#define ACTIVATE     0x1000

struct ViewPort {
    int unused;
};

struct RastPort {
    int current_pen;
};

struct BitMap {
    int unused;
};

struct Screen {
    struct ViewPort ViewPort;
    int Width;
    int Height;
    int Depth;
};

struct Window {
    struct RastPort *RPort;
    struct Screen *Screen;
    int Width;
    int Height;
};

struct NewScreen {
    short LeftEdge;
    short TopEdge;
    short Width;
    short Height;
    short Depth;
    UBYTE DetailPen;
    UBYTE BlockPen;
    unsigned short ViewModes;
    unsigned short Type;
    void *Font;
    const char *DefaultTitle;
    void *Gadgets;
    struct BitMap *CustomBitMap;
};

struct NewWindow {
    short LeftEdge;
    short TopEdge;
    short Width;
    short Height;
    UBYTE DetailPen;
    UBYTE BlockPen;
    unsigned long IDCMPFlags;
    unsigned long Flags;
    void *FirstGadget;
    void *CheckMark;
    const char *Title;
    struct Screen *Screen;
    struct BitMap *BitMap;
    short MinWidth;
    short MinHeight;
    unsigned short MaxWidth;
    unsigned short MaxHeight;
    unsigned short Type;
};

struct IntuitionBase {
    int unused;
};

struct GfxBase {
    int unused;
};

struct DosBase {
    int unused;
};

struct Screen *OpenScreen(struct NewScreen *newscreen);
void CloseScreen(struct Screen *screen);
struct Window *OpenWindow(struct NewWindow *newwindow);
void CloseWindow(struct Window *window);
struct ViewPort *ViewPortAddress(struct Window *window);
void SetPointer(struct Window *window,UWORD *pointer,short height,short width,short xoffset,short yoffset);
void SetRGB4(struct ViewPort *viewport,short index,short red,short green,short blue);
void SetAPen(struct RastPort *rp,short pen);
void WritePixel(struct RastPort *rp,short x,short y);

#endif
