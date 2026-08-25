#ifndef RT_H
#define RT_H

#include <stddef.h>

static_assert(sizeof(short) == 2, "The original Amiga short was 16-bit.");
static_assert(sizeof(int) == 4, "This port expects a 32-bit int.");
static_assert(sizeof(double) == 8, "This port keeps the original double math.");

#define BIG 1.0e10
#define SMALL 1.0e-3
#define DULL    0
#define BRIGHT  1
#define MIRROR  2

struct lamp {
    double pos[3];
    double color[3];
    double radius;
};

struct sphere {
    double pos[3];
    double color[3];
    double radius;
    int type;
};

struct patch {
    double pos[3];
    double normal[3];
    double color[3];
};

struct world {
    int numsp;
    struct sphere *sp;
    int numlmp;
    struct lamp *lmp;
    struct patch horizon[2];
    double illum[3];
    double skyhor[3];
    double skyzen[3];
};

struct observer {
    double obspos[3];
    double viewdir[3];
    double uhat[3];
    double vhat[3];
    double fl,px,py;
    int nx,ny;
};

double dot(double *a,double *b);
void setup(struct observer *o,struct world *w,int *skip,const char *datfile);
int setupfromdat(struct observer *o,struct world *w,int *skip,const char *datfile);
int raytrace(double brite[3],double *line,struct world *w);
void skybrite(double brite[3],double *line,struct world *w);
void pixline(double *line,struct observer *o,int i,int j);
void vecsub(double *a,double *b,double *c);
int intsplin(double *t,double *line,struct sphere *sp);
int qintsplin(double *line,struct sphere *sp);
int inthor(double *t,double *line);
void genline(double *l,double *a,double *b);
void point(double *pos,double t,double *line);
int glint(double brite[3],struct patch *p,struct world *w,struct sphere *spc,double *incident);
int mirror(double brite[3],struct patch *p,struct world *w,double *incident);
void pixbrite(double brite[3],struct patch *p,struct world *w,struct sphere *spc);
void setnorm(struct patch *p,struct sphere *s);
void colorcpy(double *a,double *b);
void veccopy(double *a,double *b);
int gingham(double *pos);
int reflect(double *y,double *n,double *x);
void vecprod(double *a,double *b,double *c);
int veczero(double *v);
void initsc(int width,int height);
void cleanup(const char *s);
void ham(int i,int j,double brite[3]);
void capture_rgb(int i,int j,double brite[3]);
void write_captured_png(const char *name);
void ham2(int i,int j,int pix[3]);
int nearestp(int *c,int *dist);
int coldist(int *a,int *b);
int coldist2(int *a,int *b);

#endif
