 
/*      RT3.C
        Copyright 1987 Eric Graham
        All rights reserved.
        This file may not be copied, modified or uploaded to a bulletin
        board system, except as provided below.
        Permission is granted to make a reasonable number of backup copies,
        in order that it may be used to generate executable code for use
        on a single computer system.
        Permission is granted to modify this code and use the modified code
        for non commercial use by the original purchaser of this software,
        and provided that this notice is included in the modified version.
*/

#include "rt.h"
#include "exec/types.h"
#include "exec/exec.h"
#include "intuition/intuition.h"
#include <stdio.h>
#include <stdlib.h>

static int threshhold=4;
static int nallocr=2;
static int creg[16][3]=         /* color registers */
        {{0,0,0},               /* pre-allocate black and white */
         {15,15,15},
         {0,0,0},
         {0,0,0},
         {0,0,0},
         {0,0,0},
         {0,0,0},
         {0,0,0},
         {0,0,0},
         {0,0,0},
         {0,0,0},
         {0,0,0},
         {0,0,0},
         {0,0,0},
         {0,0,0},
         {0,0,0}};



 static struct NewScreen newscreen={
   0,0,         /* Leftedge, topedge */
   320,200,     /* Width, height */
   6,           /* Depth */
   0,0,         /* pens */
   HAM,         /* ViewModes */
   CUSTOMSCREEN,
   NULL,        /* Font */
   NULL,        /* title */
   NULL,        /* gadgets */
   NULL};       /* bitmap */


 static struct NewWindow newwindow={
   0,0,              /* LeftEdge, TopEdge   */
   320,200,          /* Width, Height       */
   0,1,              /* DetailPen, BlockPen */
   0,
                     /* IDCMPFlags          */
   BORDERLESS | ACTIVATE,
                     /* Flags               */
   NULL,             /* FirstGadget         */
   NULL,             /* CheckMark           */
   NULL,             /* Title               */
   NULL,             /* Screen              */
   NULL,             /* BitMap              */
   0,0,              /* MinWidth, MinHeight */
   0,0,              /* MaxWidth, MaxHeight */
   CUSTOMSCREEN      /* Type                */
   };

 struct RastPort *rastport;

 static struct Screen *screen;
 static struct Window *window;
 struct ViewPort *viewport;

 struct IntuitionBase *IntuitionBase;
 struct GfxBase *GfxBase;
 struct DosBase *DosBase;




void initsc(int width,int height)    /* set up the screen and window */
{int i;
 static UWORD pointer[]={0,0,0,0,0,0};

 if ((GfxBase=(struct GfxBase *)
      OpenLibrary("graphics.library",0)) == NULL)
         cleanup("GfxBase");

 if ((IntuitionBase=(struct IntuitionBase *)
      OpenLibrary("intuition.library",0)) == NULL)
         cleanup("IntuitionBase");

 if ((DosBase=(struct DosBase *)
      OpenLibrary("dos.library",0)) == NULL)
         cleanup("DosBase");

 screen=OpenScreen(&newscreen);
 newwindow.Screen=screen;
 newwindow.Width=(short)width;
 newwindow.MinWidth=(short)width;
 newwindow.MaxWidth=(unsigned short)width;
 newwindow.Height=(short)height;
 newwindow.MinHeight=(short)height;
 newwindow.MaxHeight=(unsigned short)height;
 window=OpenWindow(&newwindow);
 if (!window) cleanup("OpenWindow");
 rastport=window->RPort;

 /* viewport=ViewPortAddress(window); */
 viewport=&(screen->ViewPort);
 for (i=0; i<32; ++i)
       SetRGB4(viewport,(short)i, 0,0,0);
 SetRGB4(viewport,1, 15,15,15);

 SetPointer(window,pointer,1,8,0,0);
}



void cleanup(const char *s)
{
 if (window) CloseWindow(window);
 if (screen) CloseScreen(screen);
 if (DosBase) CloseLibrary(DosBase);
 if (IntuitionBase) CloseLibrary(IntuitionBase);
 if (GfxBase) CloseLibrary(GfxBase);
 if (s) puts(s);
 exit(!!s);
}



void ham2(int i,int j,int pix[3])
{int k;
 static int prevpix[3],map[3]={0x20,0x30,0x10};
 int dif,dif2,id,maxdif,pen;

 if (!i)        /* first pixel on a line, use a register */
        {pen=nearestp(pix,&dif2);
         reg:
         for (k=0; k<3; ++k) prevpix[k]=creg[pen][k];
        }
 else
        {dif=coldist(pix,prevpix);      /* what change from last pixel*/
         if (dif)
                {pen=nearestp(pix,&dif2); /* which register is nearest */
                 if (dif2 < dif) goto reg;
                }
         id=maxdif=0;
         for (k=0; k<3; ++k)
                {dif=pix[k]-prevpix[k];
                 if (dif < 0) dif=-dif;
                 if (dif > maxdif) {maxdif=dif; id=k;}
                }
         pen=map[id]+pix[id]; prevpix[id]=pix[id];      /* use HAM */
        }
 SetAPen(rastport,(short)pen); WritePixel(rastport,(short)i,(short)j);
}


int nearestp(int *c,int *dist)
/*      Return the pen nearest to the color c.  Allocate it maybe. */
{int i,mindist,nearest,d;

 mindist=32000;
 nearest=0;

 for (i=0; i<nallocr; ++i)
        {d=coldist2(c,creg[i]);
         if (d < mindist)
                {mindist=d; nearest=i;}
        }
 if (mindist > threshhold && nallocr < 16)
        {for (i=0; i<3; ++i)
                creg[nallocr][i]=c[i];
         SetRGB4(viewport,(short)nallocr, (short)c[0],(short)c[1],(short)c[2]);
         nearest=nallocr++;
         mindist=0;
        }
 *dist=mindist;
 return nearest;
}


int coldist(int *a,int *b)    /* The 'distance' between two colors after we correct */
{int k,r,d,m;

 for (r=k=m=0; k<3; ++k)
        {d=a[k]-b[k];
         if (d < 0) d=-d;
         r+=d;
         if (d > m) m=d;
        }
 return r-m;
}


int coldist2(int *a,int *b)   /* the 'distance' between two colors */
{int k,r,d;

 for (r=k=0; k<3; ++k)
        {d=a[k]-b[k];
         if (d < 0) d=-d;
         r+=d;
        }
 return r;
}
