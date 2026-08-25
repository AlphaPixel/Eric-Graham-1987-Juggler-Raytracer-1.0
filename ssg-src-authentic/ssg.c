/*      SSG.C    Scene Simulation Generator - command line and scene reader
        Copyright 1987 Eric Graham

        Released by Eric Graham into the public domain in 2026, provided
        that Eric Graham is credited in all uses of the code.

        Source reconstruction, cross-checked against the Ghidra
        decompilation of the scene-reader function in the original ssg
        executable.  It follows the binary's actual behaviour:

          * the scene file is read straight from the stream with fscanf()
            using the executable's own format strings - it is NOT slurped
            into a buffer;
          * one AllocMem of 0x5140 bytes (room for 400 spheres) is taken
            up front, before parsing, and the object list fills it in
            place - there is no counting pass and no growing list;
          * objects are found by reading a single character and testing
            it for '<' or ';', skipping only '\n', ' ' and '\t';
          * an object header is scanned as "%lf,%lf,%lf> %d" AFTER the
            '<' has already been consumed by that character read.

        The recovered format strings and messages are used verbatim -
        (%lf,%lf,%lf), [%lf,%lf], %lf, "%lf,%lf,%lf> %d",
        (%lf,%lf,%lf):%lf, <%lf,%lf,%lf>, "\nTotal number of spheres=%d",
        "\nNumber of lamps=%d", "input file error on '%s'", "Unable to
        allocate memory", "Unable to allocate lamp memory",
        "Unexpected end of input", "\nError before:", "input error".

        Two unavoidable concessions to a modern compiler, both marked
        below: ANSI prototypes instead of K&R, and a leading space added
        to the parenthesis/angle format strings so ISO fscanf skips the
        whitespace that the Amiga C library skipped before a literal.
*/

#define _CRT_SECURE_NO_WARNINGS

#include "ssg.h"
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/*      ---------------- command line ---------------- */

static void defaults(struct options *op)
{
    memset(op,0,sizeof(*op));
    op->samp=0;  op->thresh=4;  op->blur=3;
    op->mx0=-1;  op->my0=-1;  op->mx1=10000;  op->my1=10000;
}

static int wantnum(const char *s,int *v)  /* a whole switch argument as one integer */
{
    char *end;  long n;
    n=strtol(s,&end,10);
    if (end == s || *end) return 0;
    *v=(int)n;  return 1;
}

static int getargs(struct options *op,int argc,char **argv)
{
    int i;  static char maskname[256];
    defaults(op);
    for (i=1; i<argc; ++i) {
        int sw=toupper((unsigned char)argv[i][0]);
        const char *val=argv[i]+2;
        if (sw != 'E' && sw != 'V' && argv[i][1] != '=') {
            printf("command line error %s\n",argv[i]);  return 0;
        }
        switch (sw) {
        case 'I': op->ifile=val;  break;
        case 'D': op->dfile=val;  break;
        case 'O': op->ofile=val;  break;
        case 'R': op->rfile=val;  break;
        case 'M':
            if (sscanf(val,"%d:%d:%d:%d:%255s",
                       &op->mx0,&op->mx1,&op->my0,&op->my1,maskname) != 5) {
                printf("command line error %s\n",argv[i]);  return 0;
            }
            op->mfile=maskname;  break;
        case 'T': if (!wantnum(val,&op->thresh)) return 0;  break;
        case 'S': if (!wantnum(val,&op->samp))   return 0;  break;
        case 'B': if (!wantnum(val,&op->blur))   return 0;  break;
        case 'V': op->vflag=1;  break;
        case 'E': op->eflag=1;  break;
        default:  printf("Illegal switch %s\n",argv[i]);  return 0;
        }
    }
    if (!op->ifile) {printf("No input file\n");  return 0;}
    return 1;
}

/*      ---------------- scene reader ---------------- */

/*      The binary AllocMem's 0x5140 bytes for spheres, which is room for */
/*      400 of its 0x34-byte sphere records.  We keep the same fixed cap. */
#define MAXSP 400

static FILE *df;                /* the scene file, read directly by fscanf */
static struct sphere *sp0;      /* the one up-front sphere allocation */
static int nsp;                 /* spheres stored so far */

/*      Number readers.  The leading space in the parenthesis/angle forms */
/*      is the one addition to the recovered format strings: ISO fscanf   */
/*      does not skip whitespace before a literal '(' or '<', but the     */
/*      Amiga C library did, so the original strings had no such space.   */

static int rdpos(float v[3])            /* (x,y,z) */
{
    double a,b,c;
    if (fscanf(df," (%lf,%lf,%lf)",&a,&b,&c) != 3) return 0;
    v[0]=(float)a; v[1]=(float)b; v[2]=(float)c;  return 1;
}

static int rdcol(float v[3])            /* <r,g,b> */
{
    double a,b,c;
    if (fscanf(df," <%lf,%lf,%lf>",&a,&b,&c) != 3) return 0;
    v[0]=(float)a; v[1]=(float)b; v[2]=(float)c;  return 1;
}

static int rdsphere(float v[3],float *rad)   /* (x,y,z):r */
{
    double a,b,c,r;
    if (fscanf(df," (%lf,%lf,%lf):%lf",&a,&b,&c,&r) != 4) return 0;
    v[0]=(float)a; v[1]=(float)b; v[2]=(float)c; *rad=(float)r;  return 1;
}

static int rdhdr(float col[3],int *type)     /* r,g,b> t   ('<' already read) */
{
    double a,b,c;
    if (fscanf(df,"%lf,%lf,%lf> %d",&a,&b,&c,type) != 4) return 0;
    col[0]=(float)a; col[1]=(float)b; col[2]=(float)c;  return 1;
}

static int rdint(int *v)        /* the interpolation count / lamp count */
{
    return fscanf(df," %d",v) == 1;
}

static int rdnum(float *v)      /* the focal length factor */
{
    double a;
    if (fscanf(df," %lf",&a) != 1) return 0;
    *v=(float)a;  return 1;
}

static int rdchar(int *ch)      /* one character, as the binary's %c */
{
    int c=fgetc(df);
    if (c == EOF) return 0;
    *ch=c;  return 1;
}

static int rdtoken(int *ch)     /* next character, skipping \n, space, tab */
{
    int c;
    do { c=fgetc(df); if (c == EOF) return 0; }
    while (c == '\n' || c == ' ' || c == '\t');
    *ch=c;  return 1;
}

static int addsphere(struct sphere *sp)   /* store into the fixed buffer */
{
    if (nsp >= MAXSP) return 0;   /* the binary trusts the file; we cap safely */
    sp0[nsp++]=*sp;  return 1;
}

static int addseg(struct sphere *base,float lastp[3],float lastr,
                  float nextp[3],float nextr,int count)
/*      interpolate along a segment.  The binary stores 'count'+1 spheres */
/*      from the last point up to (but not including) the next point, and */
/*      the next point itself is stored later as the following segment's  */
/*      start or as the object's final sphere.                            */
{
    int a,k;  float t;  struct sphere sp;
    if (count < 0) return 0;
    sp=*base;
    for (a=0; a<=count; ++a) {
        t=(float)a/(float)(count+1);
        for (k=0; k<3; ++k) sp.pos[k]=lastp[k]+(nextp[k]-lastp[k])*t;
        sp.radius=lastr+(nextr-lastr)*t;
        if (!addsphere(&sp)) return 0;
    }
    return 1;
}

static void setupobserver(struct observer *o,float alt,float az,float fl)
{
    float degtorad=0.0174533;
    o->nx=320;  o->ny=200;
    o->px=1.0/o->nx;  o->py=0.75/o->ny;
    o->fl=0.028*fl;
    alt*=degtorad;  az*=degtorad;
    o->viewdir[0]=cosf(az)*cosf(alt);
    o->viewdir[1]=sinf(az)*cosf(alt);
    o->viewdir[2]=sinf(alt);
    o->uhat[0]=sinf(az);
    o->uhat[1]=-cosf(az);
    o->uhat[2]=0.0;
    o->vhat[0]=-cosf(az)*sinf(alt);
    o->vhat[1]=-sinf(az)*sinf(alt);
    o->vhat[2]=cosf(alt);
}

int setupfromdat(struct observer *o,struct world *w,const char *datfile)
{
    float angles[2],fl,lastp[3],lastr,nextp[3],nextr;
    struct sphere base,sp;
    int i,k,ch,count;

    memset(w,0,sizeof(*w));
    nsp=0;  sp0=0;

    df=fopen(datfile,"r");
    if (!df) {printf("input file error on '%s'\n",datfile);  return 0;}

    if (!rdpos(o->obspos)) goto badparse;
    {   double a,b;                          /* [altitude,azimuth] */
        if (fscanf(df," [%lf,%lf]",&a,&b) != 2) goto badparse;
        angles[0]=(float)a;  angles[1]=(float)b;
    }
    if (!rdnum(&fl)) goto badparse;
    setupobserver(o,angles[0],angles[1],fl);

    sp0=(struct sphere *)malloc((size_t)MAXSP*sizeof(struct sphere));  /* AllocMem(0x5140) */
    if (!sp0) {printf("Unable to allocate memory\n");  fclose(df); df=0;  return 0;}

    for (;;) {                               /* walk the object list */
        if (!rdtoken(&ch)) goto badeof;
        if (ch == ';') break;                /* ';' ends the list */
        if (ch != '<') goto badparse;        /* anything else is junk */

        if (!rdhdr(base.color,&base.type)) goto badparse;   /* <r,g,b> type */
        if (!rdsphere(lastp,&lastr)) goto badparse;         /* first point */
        for (;;) {                           /* the interpolated segments */
            if (!rdint(&count)) break;       /* no count -> object done */
            if (!rdsphere(nextp,&nextr)) goto badparse;
            if (!addseg(&base,lastp,lastr,nextp,nextr,count)) goto badparse;
            for (k=0; k<3; ++k) lastp[k]=nextp[k];
            lastr=nextr;
        }
        sp=base;                             /* store the final point */
        for (k=0; k<3; ++k) sp.pos[k]=lastp[k];
        sp.radius=lastr;
        if (!addsphere(&sp)) goto badparse;
        if (!rdchar(&ch) || ch != ';') goto badparse;   /* the object's ';' */
    }

    w->numsp=nsp;  w->sp=sp0;  sp0=0;
    printf("\nTotal number of spheres=%d",nsp);

    if (!rdint(&w->numlmp)) goto badparse;
    printf("\nNumber of lamps=%d",w->numlmp);
    w->lmp=(struct lamp *)malloc((size_t)w->numlmp*sizeof(struct lamp));
    if (!w->lmp) {
        printf("Unable to allocate lamp memory\n");
        fclose(df); df=0;  freeworld(w);  return 0;
    }
    for (i=0; i<w->numlmp; ++i)
        if (!rdsphere(w->lmp[i].pos,&w->lmp[i].radius) ||
            !rdcol(w->lmp[i].color)) goto badparse;

    if (!rdcol(w->horizon[0].color) || !rdcol(w->horizon[1].color) ||
        !rdcol(w->illum) || !rdcol(w->skyzen) || !rdcol(w->skyhor)) goto badparse;

    for (i=0; i<2; ++i) {
        w->horizon[i].normal[0]=0.0;
        w->horizon[i].normal[1]=0.0;
        w->horizon[i].normal[2]=1.0;
        for (k=0; k<3; ++k) w->horizon[i].pos[k]=0.0;
    }
    fclose(df);  df=0;
    return 1;

badeof:                                      /* ran off the end of the file */
    printf("\nUnexpected end of input");
    printf("\ninput error\n");
    fclose(df);  df=0;  free(sp0);  sp0=0;  freeworld(w);  return 0;

badparse:                                    /* a token did not scan */
    printf("\nError before:");
    for (i=0; i<100; ++i) {
        ch=fgetc(df);
        if (ch == EOF) break;
        printf("%c",ch);
    }
    printf("\ninput error\n");
    fclose(df);  df=0;  free(sp0);  sp0=0;  freeworld(w);  return 0;
}

void freeworld(struct world *w)
{
    free(w->sp);  free(w->lmp);  free(w->bound);  memset(w,0,sizeof(*w));
}

/*      ---------------- visibility preprocessing ---------------- */

static void projectscene(struct observer *o,struct world *w,int vflag)
/*      Give every sphere and lamp its screen box and visibility flag, and */
/*      build the whole-scene bounding sphere.  The binary does this in one */
/*      pass at the end of the scene reader; we do it here for the same     */
/*      effect.                                                             */
{
    int i,k;
    for (i=0; i<w->numsp; ++i)
        project(o,w->sp[i].pos,w->sp[i].radius,vflag,&w->sp[i].flag,
                &w->sp[i].xmin,&w->sp[i].xmax,&w->sp[i].ymin,&w->sp[i].ymax);
    for (i=0; i<w->numlmp; ++i)
        project(o,w->lmp[i].pos,w->lmp[i].radius,vflag,&w->lmp[i].flag,
                &w->lmp[i].xmin,&w->lmp[i].xmax,&w->lmp[i].ymin,&w->lmp[i].ymax);

    /*  The binary allocates a bounding sphere once there are three or more */
    /*  spheres: centre at the middle of the sphere bounding box, radius    */
    /*  reaching the farthest sphere surface.  We build it identically.  Its*/
    /*  consumer was not located in the recovered render code, so it is kept */
    /*  here for load-time fidelity rather than used in the cull.           */
    w->bound=0;
    if (w->numsp >= 3) {
        float lo[3],hi[3],c[3],d[3],r,rmax;
        w->bound=(struct sphere *)calloc(1,sizeof(struct sphere));
        if (w->bound) {
            for (k=0; k<3; ++k) {lo[k]=BIG; hi[k]=-BIG;}
            for (i=0; i<w->numsp; ++i)
                for (k=0; k<3; ++k) {
                    float a=w->sp[i].pos[k]-w->sp[i].radius;
                    float b=w->sp[i].pos[k]+w->sp[i].radius;
                    if (a<lo[k]) lo[k]=a;
                    if (b>hi[k]) hi[k]=b;
                }
            for (k=0; k<3; ++k) c[k]=0.5*(lo[k]+hi[k]);
            rmax=0.0;
            for (i=0; i<w->numsp; ++i) {
                for (k=0; k<3; ++k) d[k]=w->sp[i].pos[k]-c[k];
                r=sqrtf(dot(d,d))+w->sp[i].radius;
                if (r>rmax) rmax=r;
            }
            for (k=0; k<3; ++k) w->bound->pos[k]=c[k];
            w->bound->radius=rmax;
        }
    }
}

static int actsp(struct world *w,int srcy,int *out)   /* active spheres for a scanline */
{
    int i,c,f;
    for (i=c=0; i<w->numsp; ++i) {
        f=w->sp[i].flag;
        if (f != OBEHIND &&
            (f == ONEAR || (w->sp[i].ymin <= srcy && srcy <= w->sp[i].ymax)))
            out[c++]=i;
    }
    out[c]=-1;  return c;
}

static int actlmp(struct world *w,int srcy,int *out)  /* active lamps for a scanline */
{
    int i,c,f;
    for (i=c=0; i<w->numlmp; ++i) {
        f=w->lmp[i].flag;
        if (f != OBEHIND &&
            (f == ONEAR || (w->lmp[i].ymin <= srcy && srcy <= w->lmp[i].ymax)))
            out[c++]=i;
    }
    out[c]=-1;  return c;
}

/*      ---------------- render loop ---------------- */

static int stepof(int samp)     /* S= index -> pixel step */
{
    static int map[4]={1,2,4,8};
    if (samp < 0) samp=0;  if (samp > 3) samp=3;
    return map[samp];
}

static unsigned char clampbyte(int v)
{
    if (v < 0) return 0;  if (v > 255) return 255;  return (unsigned char)v;
}

int main(int argc,char **argv)
{
    struct options op;  struct observer o;  struct world w;
    unsigned char *rgb,*mask;  int step,outw,outh,x,y,sx,sy,k,rc;
    int *spinc,*lmpinc;
    float line[6],brite[3];

    printf("\n\nSSG: Scene Simulation Generator\nCopyright 1987 Eric Graham\n");

    if (!getargs(&op,argc,argv)) return 1;
    if (!setupfromdat(&o,&w,op.ifile)) return 1;
    exposelamps(&w);
    projectscene(&o,&w,op.vflag);       /* screen boxes for the cull */

    step=stepof(op.samp);
    outw=1+(o.nx-1)/step;  outh=1+(o.ny-1)/step;
    rgb=(unsigned char *)malloc((size_t)outw*outh*3);
    spinc=(int *)malloc((size_t)(w.numsp+1)*sizeof(int));   /* per-scanline */
    lmpinc=(int *)malloc((size_t)(w.numlmp+1)*sizeof(int)); /* active lists */
    if (!rgb || !spinc || !lmpinc)
        {free(rgb); free(spinc); free(lmpinc); freeworld(&w); return 1;}

    mask=0;
    if (op.mfile) {
        FILE *mf=fopen(op.mfile,"rb");
        size_t mn=(size_t)outw*outh*3;
        mask=(unsigned char *)malloc(mn);
        if (!mf || !mask || fread(mask,1,mn,mf) != mn) {
            printf("Unable to open mask file '%s'\n",op.mfile);
            if (mf) fclose(mf);
            free(mask);  free(rgb);  freeworld(&w);  return 1;
        }
        fclose(mf);
    }

    for (sy=0,y=0; sy<o.ny; sy+=step,++y) {
        actsp(&w,sy,spinc);         /* which objects can this scanline see? */
        actlmp(&w,sy,lmpinc);
        for (sx=0,x=0; sx<o.nx; sx+=step,++x) {
            unsigned char *dst=&rgb[((size_t)y*outw+x)*3];
            int trace=1;
            if (mask) {         /* keep old pixels outside the rectangle */
                trace=(op.mx0<=x && x<=op.mx1 && op.my0<=y && y<=op.my1);
                if (!trace) {
                    memcpy(dst,&mask[((size_t)y*outw+x)*3],3);
                    printf("masked\n");
                }
            }
            if (trace) {
                pixline(line,&o,sx,sy,op.vflag);
                raytrace(brite,line,&w,spinc,lmpinc,sx);
                for (k=0; k<3; ++k)     /* 4-bit exposure, held at 8-bit here */
                    dst[k]=clampbyte((int)(128.0*brite[k]+4.0));
            }
        }
    }
    free(mask);  free(spinc);  free(lmpinc);

    rc=0;
    if (op.dfile) {             /* D= : raw rgb dump, no header */
        FILE *dfp=fopen(op.dfile,"wb");
        size_t dn=(size_t)outw*outh*3;
        if (!dfp || fwrite(rgb,1,dn,dfp) != dn) {
            printf("Unable to open dump file '%s'\n",op.dfile);  rc=1;
        }
        if (dfp) fclose(dfp);
    }
    if (op.ofile && !rc) {      /* O= : ss display file */
        if (!haminit(outw,outh,op.thresh,op.blur)) rc=1;
        if (!rc && op.rfile && !hamregs(op.rfile)) {
            printf("Unable to open register file '%s'\n",op.rfile);  rc=1;
        }
        if (!rc && !hamencode(rgb)) rc=1;
        if (!rc && !hamwrite(op.ofile)) {
            printf("Unable to open output file '%s'\n",op.ofile);  rc=1;
        }
        hamfree();
    }

    free(rgb);  freeworld(&w);
    return rc;
}
