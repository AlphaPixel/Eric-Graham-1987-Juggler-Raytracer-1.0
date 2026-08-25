/*      RAYTRACE.C   Ray tracing core of the Scene Simulation Generator
        Copyright 1987 Eric Graham

        Released by Eric Graham into the public domain in 2026, provided
        that Eric Graham is credited in all uses of the code.

        Source reconstruction: this is rt1.c's renderer as it
        lived inside ssg, in single precision.  Function names, structure
        and comments follow Eric's published rt1.c wherever it survives.
        The one place ssg genuinely differs from the rt1.c LISTING is
        reflect() - the executable uses the textbook law of reflection,
        which is what is reproduced here (see the note there).
*/

#include "ssg.h"
#include <math.h>

int intsplin(float *t,float *line,struct sphere *sp)   /* intersection of sphere and line */
{/* t returns the parameter for where on the line the sphere is hit */
    float a,b,c,d,p,q,tt;  int k;
    a=b=0.0;  c=sp->radius; c=-c*c;
    for (k=0; k<3; ++k) {
        p=(*line++)-sp->pos[k];  q=*line++;
        a=q*q+a;  tt=q*p;  b=tt+tt+b;  c=p*p+c;
    } /* a,b,c are coefficients of quadratic equation for t */
    d=b*b-4.0*a*c;
    if (d <= 0.0) return 0;     /* line misses sphere */
    d=sqrtf(d);  *t=-(b+d)/(a+a);
    if (*t<SMALL) *t=(d-b)/(a+a);
    return *t>SMALL;            /* is sphere in front of us? */
}

int inthor(float *t,float *line)   /* intersection of line with ground */
{
    if (line[5] == 0.0) return 0;
    *t=-line[4]/line[5];  return *t > SMALL;
}

void setnorm(struct patch *p,struct sphere *s)  /* normal (radial) direction of sphere */
{
    float *t,a;  int k;
    vecsub(t=p->normal,p->pos,s->pos);  a=1.0/s->radius;
    for (k=0; k<3; ++k) {*t=(*t)*a; ++t;}
}

void skybrite(float brite[3],float *line,struct world *w)   /* calculate sky color */
{   /* Blend a sky color from the zenith to the horizon */
    float sin2,cos2;  int k;
    sin2=line[5]*line[5];
    sin2/=(line[1]*line[1]+line[3]*line[3]+sin2);
    cos2=1.0-sin2;
    for (k=0; k<3; ++k)
        brite[k]=cos2*w->skyhor[k]+sin2*w->skyzen[k];
}

void pixbrite(float brite[3],struct patch *p,struct world *w,struct sphere *spc)  /* how bright is the patch? */
{
    int k,l;  float line[6],t,r,lp[3],*pp,*ll,cosi,diffuse;
    static float zenith[3]={0.0,0.0,1.0},f1=1.5,f2=0.4;
    diffuse=(dot(zenith,p->normal)+f1)*f2;      /* the sky is a hemisphere lamp */
    for (k=0; k<3; ++k) brite[k]=diffuse*w->illum[k]*p->color[k];
    for (l=0; l<w->numlmp; ++l) {
        ll=(w->lmp+l)->pos;  pp=p->pos;  vecsub(lp,ll,pp);
        cosi=dot(lp,p->normal);  if (cosi <= 0.0) goto cont;
        genline(line,pp,ll);
        for (k=0; k<w->numsp; ++k) {
            if (w->sp+k == spc) continue;       /* don't shadow yourself */
            if (intsplin(&t,line,w->sp+k)) goto cont;
        }
        r=sqrtf(dot(lp,lp));  cosi=cosi/(r*r*r);  /* Lambert times 1/r^2 */
        for (k=0; k<3; ++k)
            brite[k]=brite[k]+cosi*p->color[k]*w->lmp[l].color[k];
        cont:
            ;
    }
}

int glint(float brite[3],struct patch *p,struct world *w,struct sphere *spc,float *incident)
{   /* is it a highlight? */
    int k,l,firstlite;  static float minglint=0.95;
    float line[6],t,r,lp[3],*pp,*ll,cosi;
    float incvec[3],refvec[3],ref2;
    firstlite=1;
    for (l=0; l<w->numlmp; ++l) {
        ll=(w->lmp+l)->pos;  pp=p->pos;
        vecsub(lp,ll,pp);  cosi=dot(lp,p->normal);
        if (cosi <= 0.0) continue;      /* not with this lamp! */
        genline(line,pp,ll);
        for (k=0; k<w->numsp; ++k) {
            if (w->sp+k == spc) continue;
            if (intsplin(&t,line,w->sp+k)) goto cont;
        }
        if (firstlite) {
            incvec[0]=incident[1];  incvec[1]=incident[3];
            incvec[2]=incident[5];
            reflect(refvec,p->normal,incvec);
            ref2=dot(refvec,refvec);  firstlite=0;
        }
        r=dot(lp,lp);  t=dot(lp,refvec);
        t*=t/(r*ref2);
        if (t > minglint) {     /* it's a highlight */
            for (k=0; k<3; ++k) brite[k]=1.0;
            return 1;
        }
        cont:
            ;
    }
    return 0;
}

void mirror(float brite[3],struct patch *p,struct world *w,float *incident)  /* bounce ray off mirror */
{
    int k;  float line[6],incvec[3],refvec[3],t;
    incvec[0]=incident[1];  incvec[1]=incident[3];
    incvec[2]=incident[5];  t=dot(p->normal,incvec);
    if (t >= 0.0) {     /* we're inside a sphere, it's dark */
        for (k=0; k<3; ++k) brite[k]=0.0;
        return;
    }
    reflect(refvec,p->normal,incvec);  line[0]=p->pos[0];
    line[2]=p->pos[1];  line[4]=p->pos[2];  line[1]=refvec[0];
    line[3]=refvec[1];  line[5]=refvec[2];
    raytrace(brite,line,w,0,0,0);       /* recursion saves the day (no cull) */
    for (k=0; k<3; ++k) brite[k]=brite[k]*p->color[k];
}

void pixline(float *line,struct observer *o,int i,int j,int vflag)  /* calculate ray for pixel i,j */
{
    float x,y,tp[3];  int k;
    if (!vflag) {
        y=(0.5*o->ny-j)*o->py;
        x=(i-0.5*o->nx)*o->px;
        for (k=0; k<3; ++k)
            tp[k]=o->viewdir[k]*o->fl+y*o->vhat[k]+
                  x*o->uhat[k]+o->obspos[k];
    } else {            /* V: swap the two screen basis vectors */
        y=(j-0.5*o->ny)*o->py;
        x=(i-0.5*o->nx)*o->px;
        for (k=0; k<3; ++k)
            tp[k]=o->viewdir[k]*o->fl+y*o->uhat[k]+
                  x*o->vhat[k]+o->obspos[k];
    }
    genline(line,o->obspos,tp);         /* generate equation of line */
}

int raytrace(float brite[3],float *line,struct world *w,
             int *spinc,int *lmpinc,int srcx)  /* Do the raytracing */
{
    float t,tmin,pos[3];  int k,m;
    struct patch ptch;  struct sphere *spnear;
    struct lamp *lmpnear;

    tmin=BIG;  spnear=0;        /* can we see some spheres */
    if (spinc) {               /* primary ray: only the scanline's actives, */
        for (m=0; (k=spinc[m]) >= 0; ++m) {         /* culled by screen column */
            if (srcx < w->sp[k].xmin || srcx > w->sp[k].xmax) continue;
            if (intsplin(&t,line,w->sp+k)) {
                if (t<tmin) {tmin=t; spnear=w->sp+k;}
            }
        }
    } else {                   /* mirror bounce: test them all */
        for (k=0; k<w->numsp; ++k)
            if (intsplin(&t,line,w->sp+k)) {
                if (t<tmin) {tmin=t; spnear=w->sp+k;}
            }
    }
    lmpnear=0;                  /* are we looking at a lamp */
    if (lmpinc) {
        for (m=0; (k=lmpinc[m]) >= 0; ++m) {
            if (srcx < w->lmp[k].xmin || srcx > w->lmp[k].xmax) continue;
            if (intsplin(&t,line,(struct sphere *)(void *)(w->lmp+k))) {
                if (t < tmin) {tmin=t; lmpnear=w->lmp+k;}
            }
        }
    } else {
        for (k=0; k<w->numlmp; ++k)
            if (intsplin(&t,line,(struct sphere *)(void *)(w->lmp+k))) {
                if (t < tmin) {tmin=t; lmpnear=w->lmp+k;}
            }
    }
    if (lmpnear) {              /* we see a lamp! */
        for (k=0; k<3; ++k)
            brite[k]=lmpnear->color[k]/(lmpnear->radius*
                     lmpnear->radius);
        return 0;
    }
    if (inthor(&t,line))        /* do we see the ground? */
        if (t<tmin) {
            point(pos,t,line);  k=gingham(pos); /* cheap vinyl */
            veccopy(w->horizon[k].pos,pos);
            pixbrite(brite,&(w->horizon[k]),w,0);
            return 0;
        }
    if (spnear) {               /* we see a sphere */
        point(ptch.pos,tmin,line);  setnorm(&ptch,spnear);
        colorcpy(ptch.color,spnear->color);
        switch(spnear->type) {  /* treat the surface type */
            case BRIGHT:        /* is it a highlight? */
                if (glint(brite,&ptch,w,spnear,line)) return 0;
            case DULL:
                pixbrite(brite,&ptch,w,spnear); return 0;
            case MIRROR:
                mirror(brite,&ptch,w,line); return 0;
        }
        return 0;
    }
    skybrite(brite,line,w);     /* nothing else, must be sky */
    return 0;
}

void exposelamps(struct world *w)   /* modify lamp brightness for the right exposure */
{
    float t,r,tp[3],lampfac;  int i,j,k;
    lampfac=BIG;
    for (i=0; i<w->numsp; ++i)
        for (j=0; j<w->numlmp; ++j) {
            vecsub(tp,w->sp[i].pos,w->lmp[j].pos);
            r=sqrtf(dot(tp,tp));
            r-=w->sp[i].radius;
            for (k=0; k<3; ++k) {
                t=w->sp[i].color[k]*w->lmp[j].color[k]/(r*r);
                if (t == 0.0) continue;
                t=(1.0-w->sp[i].color[k]*w->illum[k])/t;
                if (t<lampfac) lampfac=t;
            }
        }
    for (j=0; j<w->numlmp; ++j)
        for (k=0; k<3; ++k)
            w->lmp[j].color[k]*=lampfac;
    printf("\nlampfac=%f",lampfac);
}

/*      -------- vector arithmetic (all of rt1.c's little helpers) -------- */

void vecsub(float *a,float *b,float *c)  /* a=b-c for vectors */
{
    int k;
    for (k=0; k<3; ++k) a[k]=b[k]-c[k];
}

void genline(float *l,float *a,float *b)  /* equation of a line through two points */
{
    int k;
    for (k=0; k<3; ++k) {*l++=a[k]; *l++=b[k]-a[k];}
}

float dot(float *a,float *b)    /* dot product of 2 vectors */
{
    return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
}

void point(float *pos,float t,float *line)  /* position of a point on the line */
{
    int k;  float a;
    for (k=0; k<3; ++k) {
        a=*line++;  pos[k]=a+(*line++)*t;
    }
}

void colorcpy(float *a,float *b)  /* a=b for colors */
{
    int k;
    for (k=0; k<3; ++k) a[k]=b[k];
}

void veccopy(float *a,float *b)  /* a=b for vectors */
{
    int k;
    for (k=0; k<3; ++k) a[k]=b[k];
}

int gingham(float *pos)         /* are we on 'black' or 'white' tile? */
{       /* tiles are 3 units wide */
    float x,y;  int kx,ky;
    kx=ky=0;  x=pos[0]; y=pos[1];
    if (x < 0.0) {x=-x; ++kx;}
    if (y < 0.0) {y=-y; ++ky;}
    return ((((int)x)+kx)/3+(((int)y)+ky)/3)%2;
}

void vecprod(float *a,float *b,float *c)  /* vector product a=b^c */
{
    a[0]=b[1]*c[2]-b[2]*c[1];
    a[1]=b[2]*c[0]-b[0]*c[2];
    a[2]=b[0]*c[1]-b[1]*c[0];
}

int veczero(float *v)           /* is vector null? */
{
    if (v[0] != 0.0) return 0;  if (v[1] != 0.0) return 0;
    if (v[2] != 0.0) return 0;  return 1;
}

int reflect(float *y,float *n,float *x)  /* law of reflection, n is unit normal */
{
    float u[3],xn;  int k;
    /* NB: the rt1.c listing has a garbled reflect(); the ssg executable */
    /* uses the ordinary  y = x - 2(x.n)n , reproduced here.             */
    vecprod(u,x,n);             /* normal to the plane of n and x */
    if (veczero(u)) {           /* grazing - bounce right back */
        y[0]=-x[0];  y[1]=-x[1];  y[2]=-x[2];  return 0;
    }
    xn=dot(x,n);
    for (k=0; k<3; ++k) y[k]=x[k]-2.0*xn*n[k];
    return 0;
}

/*      -------- screen projection, for the visibility cull -------- */
/*      These invert pixline: a point p (relative to the observer) is */
/*      perspective-divided and mapped to the pixel it falls on.  The */
/*      V flag swaps the uhat/vhat screen axes, exactly as pixline does. */

static int projx(struct observer *o,float *p,int vflag)   /* -> screen column */
{
    float depth=dot(p,o->viewdir),xn;
    if (!vflag) xn=o->fl*dot(p,o->uhat)/depth;
    else        xn=o->fl*dot(p,o->vhat)/depth;
    return (int)(0.5*o->nx+xn/o->px);
}

static int projy(struct observer *o,float *p,int vflag)   /* -> screen row */
{
    float depth=dot(p,o->viewdir),yn;
    if (!vflag) {yn=o->fl*dot(p,o->vhat)/depth;  return (int)(0.5*o->ny-yn/o->py);}
    yn=o->fl*dot(p,o->uhat)/depth;               return (int)(0.5*o->ny+yn/o->py);
}

void project(struct observer *o,float *pos,float radius,int vflag,
             int *flag,int *xmin,int *xmax,int *ymin,int *ymax)
/*      classify one object and, if it is wholly in front, work out the */
/*      screen box of its silhouette.  A behind/straddling object cannot */
/*      be box-culled and is flagged so the render loop always tests it. */
{
    float v[3],n[3],perp[3],p1[3],p2[3],depth,len;
    int a,b,k;

    vecsub(v,pos,o->obspos);             /* object relative to observer */
    depth=dot(o->viewdir,v);
    if (radius+depth < 0.0) {*flag=OBEHIND; return;}   /* wholly behind us */
    if (depth-radius <= 0.0) {*flag=ONEAR; return;}    /* too close to cull */
    *flag=OFRONT;

    len=sqrtf(dot(v,v));                 /* n = unit vector to the centre */
    for (k=0; k<3; ++k) n[k]=v[k]/len;

    /* horizontal silhouette offset = radius * (n x vhat), uhat<->vhat if V */
    if (!vflag) vecprod(perp,n,o->vhat);
    else        vecprod(perp,o->uhat,n);
    for (k=0; k<3; ++k) {perp[k]*=radius; p1[k]=v[k]+perp[k]; p2[k]=v[k]-perp[k];}
    a=projx(o,p1,vflag);  b=projx(o,p2,vflag);
    *xmin=(a<b?a:b)-1;  *xmax=(a>b?a:b)+1;

    /* vertical silhouette offset = radius * (uhat x n), uhat<->vhat if V */
    if (!vflag) vecprod(perp,o->uhat,n);
    else        vecprod(perp,o->vhat,n);
    for (k=0; k<3; ++k) {perp[k]*=radius; p1[k]=v[k]+perp[k]; p2[k]=v[k]-perp[k];}
    a=projy(o,p1,vflag);  b=projy(o,p2,vflag);
    *ymin=(a<b?a:b)-1;  *ymax=(a>b?a:b)+1;
}
