/*      SSG.H
        Scene Simulation Generator - shared declarations
        Copyright 1987 Eric Graham

        Released by Eric Graham into the public domain in 2026, provided
        that Eric Graham is credited in all uses of the code.

        ---------------------------------------------------------------
        This is a source-level reconstruction of SSG, recovered from the
        1987 Amiga 68000 executable.  Its goal is not clean modern C but
        the closest plausible recovery of Eric's ORIGINAL source: his own
        names, his dense formatting, his terse comments, single-precision
        (FFP) math, and the scene-file grammar exactly as the binary's own
        scanf format strings reveal it.  Where his published
        rt1.c/rt2.c/rt3.c survive, the names and comments here are taken
        from them verbatim.  Where they do not, the names follow his
        conventions as closely as the recovered executable allows.

        The one deliberate concession to portability is ANSI function
        prototypes in place of Eric's original K&R declarations - the
        same minimal change the published rt port made so the files
        build on a modern compiler.  Everything it computes is verified
        byte-for-byte against the original Amiga executable's output.
*/

#ifndef SSG_H
#define SSG_H

#include <stdio.h>

#define BIG   1.0e10
#define SMALL 1.0e-3

#define DULL    0
#define BRIGHT  1
#define MIRROR  2

/*      The Amiga ssg linked mathffp/mathtrans, so the world is single */
/*      precision.  (The published rt used double and a different lib.) */

/*      Both records carry, after the geometry, the screen-space cull */
/*      data that setupfromdat's projection pass fills in: a visibility */
/*      flag and an inclusive screen bounding box.  The field order and */
/*      the resulting 0x34-byte sphere / 0x30-byte lamp records match */
/*      the layout the original ssg executable allocates and indexes. */

#define OBEHIND (-1)    /* wholly behind the observer - never drawn      */
#define ONEAR    0      /* straddles the observer - cannot be box-culled */
#define OFRONT   1      /* wholly in front - cull by its screen box      */

struct lamp {
    float pos[3];       /* 0x00 */
    float color[3];     /* 0x0c */
    float radius;       /* 0x18 */
    int flag;           /* 0x1c  OBEHIND / ONEAR / OFRONT */
    int ymin,ymax;      /* 0x20  vertical screen extent   */
    int xmin,xmax;      /* 0x28  horizontal screen extent */
};

struct sphere {
    float pos[3];       /* 0x00 */
    float color[3];     /* 0x0c */
    float radius;       /* 0x18 */
    int flag;           /* 0x1c */
    int ymin,ymax;      /* 0x20 */
    int xmin,xmax;      /* 0x28 */
    int type;           /* 0x30 */
};

struct patch {
    float pos[3];
    float normal[3];
    float color[3];
};

struct world {
    int numsp;   struct sphere *sp;
    int numlmp;  struct lamp *lmp;
    struct patch horizon[2];
    float illum[3];
    float skyhor[3];
    float skyzen[3];
    struct sphere *bound;       /* whole-scene bounding sphere, or 0 */
};

struct observer {
    float obspos[3];
    float viewdir[3];
    float uhat[3];
    float vhat[3];
    float fl,px,py;
    int nx,ny;
};

/*      command-line switches, one letter each */
struct options {
    const char *ifile;   /* I= input scene */
    const char *dfile;   /* D= raw rgb dump */
    const char *ofile;   /* O= ss display file */
    const char *rfile;   /* R= register/palette seed */
    const char *mfile;   /* M= mask/reuse rgb */
    int mx0,mx1,my0,my1; /* M= inclusive render rectangle */
    int samp;            /* S= sample step index 0..3 */
    int thresh;          /* T= palette allocation threshold */
    int blur;            /* B= ham smoothing amount */
    int vflag;           /* V  swap projection axes */
    int eflag;           /* E  no wait for input */
};

/*      raytracer core (raytrace.c) - see rt1.c for the double version. */
/*      spinc/lmpinc are the scanline's active-object index lists (each  */
/*      terminated by -1) and srcx is the screen column, used to cull    */
/*      spheres by their screen box.  Pass 0,0,0 for an unculled search  */
/*      (mirror bounces, which are not screen-aligned).                  */
int   raytrace(float brite[3],float *line,struct world *w,
               int *spinc,int *lmpinc,int srcx);
void  skybrite(float brite[3],float *line,struct world *w);
void  pixline(float *line,struct observer *o,int i,int j,int vflag);
void  project(struct observer *o,float *pos,float radius,int vflag,
              int *flag,int *xmin,int *xmax,int *ymin,int *ymax);
void  vecsub(float *a,float *b,float *c);
int   intsplin(float *t,float *line,struct sphere *sp);
int   inthor(float *t,float *line);
void  genline(float *l,float *a,float *b);
float dot(float *a,float *b);
void  point(float *pos,float t,float *line);
int   glint(float brite[3],struct patch *p,struct world *w,struct sphere *spc,float *incident);
void  mirror(float brite[3],struct patch *p,struct world *w,float *incident);
void  pixbrite(float brite[3],struct patch *p,struct world *w,struct sphere *spc);
void  setnorm(struct patch *p,struct sphere *s);
void  colorcpy(float *a,float *b);
void  veccopy(float *a,float *b);
int   gingham(float *pos);
int   reflect(float *y,float *n,float *x);
void  vecprod(float *a,float *b,float *c);
int   veczero(float *v);
void  exposelamps(struct world *w);

/*      scene reader and driver (ssg.c) */
int   setupfromdat(struct observer *o,struct world *w,const char *datfile);
void  freeworld(struct world *w);

/*      hold-and-modify output (ham.c) - see rt3.c for ham2/nearestp/etc */
int   haminit(int width,int height,int thresh,int blur);
void  hamfree(void);
int   hamregs(const char *name);
int   hamencode(unsigned char *rgb);   /* whole D-dump image -> bitplanes */
int   hamwrite(const char *name);
void  ham2(int i,int j,int pix[3]);
int   nearestp(int *c,int *dist);
int   coldist(int *a,int *b);
int   coldist2(int *a,int *b);

#endif
