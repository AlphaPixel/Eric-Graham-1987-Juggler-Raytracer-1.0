/*      HAM.C    Hold-and-modify palette/bitplane output for ssg
        Copyright 1987 Eric Graham

        Released by Eric Graham into the public domain in 2026, provided
        that Eric Graham is credited in all uses of the code.

        Source reconstruction.  ham2(), nearestp(), coldist()
        and coldist2() are Eric's rt3.c routines, kept name-for-name,
        including the 'threshhold' spelling and the creg/nallocr colour
        register state.  ssg's HAM path has two pieces rt3.c does not:
        a small pseudo-random dither before the 4-bit crush, and an
        optional B= blur over three buffered scanlines.  Those were
        recovered from the executable and are marked below.  Where rt3.c
        drove SetAPen()/WritePixel(), this writes straight into six
        Amiga bitplanes and out to the ss display file.
*/

#include "ssg.h"
#include <stdlib.h>
#include <string.h>

static int threshhold=4;        /* T= : distance before a new register  */
static int nallocr=2;           /* registers allocated so far           */
static int bluramt=3;           /* B= : ham smoothing amount             */
static int creg[16][3]=         /* color registers, black and white first */
        {{0,0,0},{15,15,15}};

static int hwidth,hheight,rowbytes;
static unsigned char *plane[6];
static unsigned char *quant;    /* the whole image crushed to 4 bits/chan */

int coldist(int *a,int *b)      /* 'distance' between two colors, best channel free */
{
    int k,r,d,m;
    for (r=k=m=0; k<3; ++k) {
        d=a[k]-b[k];  if (d < 0) d=-d;
        r+=d;  if (d > m) m=d;   /* one HAM pixel fixes one channel exactly */
    }
    return r-m;
}

int coldist2(int *a,int *b)     /* plain 'distance' between two colors */
{
    int k,r,d;
    for (r=k=0; k<3; ++k) {
        d=a[k]-b[k];  if (d < 0) d=-d;  r+=d;
    }
    return r;
}

int nearestp(int *c,int *dist)  /* Return the pen nearest to color c.  Allocate it maybe. */
{
    int i,mindist,nearest,d;
    mindist=32000;  nearest=0;
    for (i=0; i<nallocr; ++i) {
        d=coldist2(c,creg[i]);
        if (d < mindist) {mindist=d; nearest=i;}
    }
    if (mindist > threshhold && nallocr < 16) {
        for (i=0; i<3; ++i) creg[nallocr][i]=c[i];
        nearest=nallocr++;  mindist=0;
    }
    *dist=mindist;  return nearest;
}

static void plotpix(int i,int j,int pen)   /* poke one pen into the six bitplanes */
{
    int p,byte,mask;
    byte=j*rowbytes+i/8;  mask=0x80 >> (i & 7);
    for (p=0; p<6; ++p)
        if (pen & (1<<p)) plane[p][byte]|=(unsigned char)mask;
}

void ham2(int i,int j,int pix[3])
{
    static int prevpix[3],map[3]={0x20,0x30,0x10};
    int k,dif,dif2,id,maxdif,pen;
    if (!i) {           /* first pixel on a line, use a register */
        pen=nearestp(pix,&dif2);
        reg:
        for (k=0; k<3; ++k) prevpix[k]=creg[pen][k];
    } else {
        dif=coldist(pix,prevpix);       /* what change from last pixel */
        if (dif) {
            pen=nearestp(pix,&dif2);     /* which register is nearest */
            if (dif2 < dif) goto reg;
        }
        id=maxdif=0;
        for (k=0; k<3; ++k) {
            dif=pix[k]-prevpix[k];  if (dif < 0) dif=-dif;
            if (dif > maxdif) {maxdif=dif; id=k;}
        }
        pen=map[id]+pix[id];  prevpix[id]=pix[id];      /* use HAM */
    }
    plotpix(i,j,pen);
}

/*      ---- the two pieces ssg has and rt3.c does not (recovered) ---- */

static unsigned long rndseed;

static int hamdither(void)       /* small +/- dither before the 4-bit crush */
{
    unsigned long v;
    rndseed=(rndseed*3677u+3u)%32768u;
    v=rndseed >> 11;
    if (v & 8u) return -(int)(v & 3u);
    return (int)(v & 3u);
}

static int clampf(int v)         /* clamp to the 0..15 register range */
{
    if (v < 0) return 0;  if (v > 15) return 15;  return v;
}

static void smooth(int pix[3],int i,int j)  /* B= blur of one pixel over its 8 neighbours */
{
    int k,dx,dy,center,sum,v;
    unsigned char *q=quant;
    for (k=0; k<3; ++k) pix[k]=q[(j*hwidth+i)*3+k];
    if (i<=0 || j<=0 || i>=hwidth-1 || j>=hheight-1) return;
    for (k=0; k<3; ++k) {
        center=pix[k];  sum=0;
        for (dy=-1; dy<=1; ++dy)
            for (dx=-1; dx<=1; ++dx) {
                if (!dx && !dy) continue;
                sum+=q[((j+dy)*hwidth+(i+dx))*3+k];
            }
        v=(bluramt*sum+(16-bluramt)*8*center+64)/128;
        pix[k]=clampf(v);
    }
}

int haminit(int width,int height,int thresh,int blur)
{
    int p;
    hwidth=width;  hheight=height;
    rowbytes=((width+15)/16)*2;
    threshhold=thresh;  bluramt=blur;
    nallocr=2;
    creg[0][0]=creg[0][1]=creg[0][2]=0;
    creg[1][0]=creg[1][1]=creg[1][2]=15;
    for (p=0; p<6; ++p) {
        plane[p]=(unsigned char *)calloc((size_t)rowbytes*height,1);
        if (!plane[p]) {hamfree(); return 0;}
    }
    return 1;
}

void hamfree(void)
{
    int p;
    for (p=0; p<6; ++p) {free(plane[p]); plane[p]=0;}
    free(quant);  quant=0;
}

int hamregs(const char *name)   /* R= : seed the registers from an existing ss file */
{
    FILE *f;  unsigned char hdr[52];  int i;
    if (!(f=fopen(name,"rb"))) return 0;
    if (fread(hdr,1,52,f) != 52) {fclose(f); return 0;}
    fclose(f);
    for (i=0; i<16; ++i) {
        creg[i][0]=hdr[4+i*3+0];
        creg[i][1]=hdr[4+i*3+1];
        creg[i][2]=hdr[4+i*3+2];
    }
    nallocr=16;
    return 1;
}

int hamencode(unsigned char *rgb)   /* crush, dither, blur and HAM the whole picture */
{
    int i,j,k,v,pix[3];
    quant=(unsigned char *)malloc((size_t)hwidth*hheight*3);
    if (!quant) return 0;
    rndseed=1;
    for (j=0; j<hheight; ++j)           /* first crush 8 bits -> 4 with dither */
        for (i=0; i<hwidth; ++i)
            for (k=0; k<3; ++k) {
                v=rgb[(j*hwidth+i)*3+k]+hamdither();
                v/=8;  quant[(j*hwidth+i)*3+k]=(unsigned char)clampf(v);
            }
    for (j=0; j<hheight; ++j)           /* then blur and hold-and-modify */
        for (i=0; i<hwidth; ++i) {
            smooth(pix,i,j);
            ham2(i,j,pix);
        }
    free(quant);  quant=0;
    return 1;
}

static int putw2(FILE *f,int v)         /* big-endian 16-bit */
{
    unsigned char b[2];
    b[0]=(unsigned char)((v>>8)&255);  b[1]=(unsigned char)(v&255);
    return fwrite(b,1,2,f) == 2;
}

int hamwrite(const char *name)  /* write the ss display file */
{
    FILE *f;  int i,p;  size_t psz;
    if (!(f=fopen(name,"wb"))) return 0;
    if (!putw2(f,hwidth) || !putw2(f,hheight)) {fclose(f); return 0;}
    for (i=0; i<16; ++i) {
        unsigned char c[3];
        c[0]=(unsigned char)creg[i][0];
        c[1]=(unsigned char)creg[i][1];
        c[2]=(unsigned char)creg[i][2];
        if (fwrite(c,1,3,f) != 3) {fclose(f); return 0;}
    }
    psz=(size_t)rowbytes*hheight;
    for (p=0; p<6; ++p)
        if (fwrite(plane[p],1,psz,f) != psz) {fclose(f); return 0;}
    fclose(f);
    return 1;
}
