/*      PNG.C
        Debug capture of pre-HAM raytrace RGB values.
*/

#define _CRT_SECURE_NO_WARNINGS

#include "rt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct pngbuf {
    unsigned char *p;
    size_t n;
    size_t cap;
};

static unsigned char *rgbbuf;
static int allocx,allocy,maxx=-1,maxy=-1;

static unsigned long crc_table[256];
static int crc_ready;

static int putbyte(struct pngbuf *b,int v)
{
    unsigned char *p;
    size_t cap;

    if (b->n == b->cap) {
        cap=b->cap ? b->cap*2 : 4096;
        p=(unsigned char *)realloc(b->p,cap);
        if (!p) return 0;
        b->p=p;
        b->cap=cap;
    }
    b->p[b->n++]=(unsigned char)v;
    return 1;
}

static int putdata(struct pngbuf *b,const unsigned char *p,size_t n)
{
    while (n--)
        if (!putbyte(b,*p++)) return 0;
    return 1;
}

static int putbe32(struct pngbuf *b,unsigned long v)
{
    return putbyte(b,(int)((v>>24)&255)) &&
           putbyte(b,(int)((v>>16)&255)) &&
           putbyte(b,(int)((v>>8)&255)) &&
           putbyte(b,(int)(v&255));
}

static void make_crc_table(void)
{
    unsigned long c;
    int n,k;

    for (n=0; n<256; ++n) {
        c=(unsigned long)n;
        for (k=0; k<8; ++k)
            c=(c&1) ? 0xedb88320UL^(c>>1) : c>>1;
        crc_table[n]=c;
    }
    crc_ready=1;
}

static unsigned long update_crc(unsigned long crc,const unsigned char *buf,size_t len)
{
    if (!crc_ready) make_crc_table();
    while (len > 0) {
        crc=crc_table[(crc^*buf++)&255]^(crc>>8);
        --len;
    }
    return crc;
}

static unsigned long crc(const unsigned char *type,const unsigned char *data,size_t len)
{
    unsigned long c;

    c=update_crc(0xffffffffUL,type,4);
    c=update_crc(c,data,len);
    return c^0xffffffffUL;
}

static unsigned long adler(const unsigned char *data,size_t len)
{
    unsigned long a,b;

    a=1;
    b=0;
    while (len--) {
        a=(a+*data++)%65521UL;
        b=(b+a)%65521UL;
    }
    return (b<<16)|a;
}

static int chunk(FILE *f,const char type[4],const unsigned char *data,size_t len)
{
    unsigned char hdr[8],cbuf[4];
    unsigned long c;

    hdr[0]=(unsigned char)((len>>24)&255);
    hdr[1]=(unsigned char)((len>>16)&255);
    hdr[2]=(unsigned char)((len>>8)&255);
    hdr[3]=(unsigned char)(len&255);
    memcpy(hdr+4,type,4);
    if (fwrite(hdr,1,8,f) != 8) return 0;
    if (len && fwrite(data,1,len,f) != len) return 0;
    c=crc((const unsigned char *)type,data,len);
    cbuf[0]=(unsigned char)((c>>24)&255);
    cbuf[1]=(unsigned char)((c>>16)&255);
    cbuf[2]=(unsigned char)((c>>8)&255);
    cbuf[3]=(unsigned char)(c&255);
    return fwrite(cbuf,1,4,f) == 4;
}

static int grow(int x,int y)
{
    unsigned char *n;
    int nx,ny,row;

    nx=allocx ? allocx : 1;
    ny=allocy ? allocy : 1;
    while (x >= nx) nx*=2;
    while (y >= ny) ny*=2;
    if (nx == allocx && ny == allocy) return 1;

    n=(unsigned char *)calloc((size_t)nx*(size_t)ny*3,1);
    if (!n) return 0;
    for (row=0; row<allocy; ++row)
        memcpy(n+(size_t)row*(size_t)nx*3,
               rgbbuf+(size_t)row*(size_t)allocx*3,
               (size_t)allocx*3);
    free(rgbbuf);
    rgbbuf=n;
    allocx=nx;
    allocy=ny;
    return 1;
}

static unsigned char bytecolor(double v)
{
    int c;

    c=(int)(v*255.0+0.5);
    if (c < 0) c=0;
    if (c > 255) c=255;
    return (unsigned char)c;
}

void capture_rgb(int i,int j,double brite[3])
{
    size_t off;

    if (i < 0 || j < 0) return;
    if (!grow(i,j)) return;
    off=((size_t)j*(size_t)allocx+(size_t)i)*3;
    rgbbuf[off+0]=bytecolor(brite[0]);
    rgbbuf[off+1]=bytecolor(brite[1]);
    rgbbuf[off+2]=bytecolor(brite[2]);
    if (i > maxx) maxx=i;
    if (j > maxy) maxy=j;
}

static int filtered(struct pngbuf *out,int width,int height)
{
    int y;

    for (y=0; y<height; ++y) {
        if (!putbyte(out,0)) return 0;
        if (!putdata(out,rgbbuf+(size_t)y*(size_t)allocx*3,
                     (size_t)width*3)) return 0;
    }
    return 1;
}

static int zlibstored(struct pngbuf *out,const unsigned char *data,size_t len)
{
    size_t n,off;
    int final;
    unsigned long a;

    if (!putbyte(out,0x78) || !putbyte(out,0x01)) return 0;
    off=0;
    do {
        n=len-off;
        if (n > 65535) n=65535;
        final=(off+n == len);
        if (!putbyte(out,final)) return 0;
        if (!putbyte(out,(int)(n&255)) ||
            !putbyte(out,(int)((n>>8)&255)) ||
            !putbyte(out,(int)((~n)&255)) ||
            !putbyte(out,(int)(((~n)>>8)&255))) return 0;
        if (!putdata(out,data+off,n)) return 0;
        off+=n;
    } while (off < len);
    a=adler(data,len);
    return putbe32(out,a);
}

void write_captured_png(const char *name)
{
    static const unsigned char sig[8]={137,80,78,71,13,10,26,10};
    FILE *f;
    struct pngbuf raw,idat;
    unsigned char ihdr[13];
    int width,height;

    if (!rgbbuf || maxx < 0 || maxy < 0) return;
    width=maxx+1;
    height=maxy+1;
    raw.p=idat.p=0;
    raw.n=idat.n=raw.cap=idat.cap=0;

    if (!filtered(&raw,width,height)) goto done;
    if (!zlibstored(&idat,raw.p,raw.n)) goto done;

    f=fopen(name,"wb");
    if (!f) goto done;
    ihdr[0]=(unsigned char)((width>>24)&255);
    ihdr[1]=(unsigned char)((width>>16)&255);
    ihdr[2]=(unsigned char)((width>>8)&255);
    ihdr[3]=(unsigned char)(width&255);
    ihdr[4]=(unsigned char)((height>>24)&255);
    ihdr[5]=(unsigned char)((height>>16)&255);
    ihdr[6]=(unsigned char)((height>>8)&255);
    ihdr[7]=(unsigned char)(height&255);
    ihdr[8]=8;
    ihdr[9]=2;
    ihdr[10]=0;
    ihdr[11]=0;
    ihdr[12]=0;
    if (fwrite(sig,1,8,f) == 8 &&
        chunk(f,"IHDR",ihdr,13) &&
        chunk(f,"IDAT",idat.p,idat.n) &&
        chunk(f,"IEND",NULL,0))
        printf("\nwrote image.png");
    fclose(f);

done:
    free(raw.p);
    free(idat.p);
    free(rgbbuf);
    rgbbuf=0;
    allocx=allocy=0;
    maxx=maxy=-1;
}
