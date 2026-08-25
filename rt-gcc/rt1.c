/*      RT1.C    Ray tracing program
        Copyright 1987 Eric Graham
        Permission is granted to copy and modify this file, provided
        that this notice is retained.
*/


#include <math.h>
#include <string.h>
#include <proto/dos.h>

#define BIG 1.0e10
#define SMALL 1.0e-3
#define DULL    0
#define BRIGHT  1
#define MIRROR  2

double dot();   /* Vector dot product */
struct lamp {
    double pos[3];      /* position of lamp */
    double color[3];    /* color of lamp */
    double radius;      /* size of lamp */
};

struct sphere {
    double pos[3];      /* position of sphere */
    double color[3];    /* color of sphere */
    double radius;      /* size of sphere */
    int type;   /* type of surface, DULL, BRIGHT or MIRROR */
};

struct patch {          /* a small bit of something visible */
    double pos[3];      /* position */
    double normal[3];   /* direction 90 degrees to surface */
    double color[3];    /* color of patch */
};

struct world {  /* everything in the universe, except observer */
    int numsp;          /* number of spheres */
    struct sphere *sp;  /* array of spheres */
    int numlmp;         /* number of lamps */
    struct lamp *lmp;   /* array of lamps */
    struct patch horizon[2]; /* alternate squares on the ground */
    double illum[3];    /* background diffuse illumination */
    double skyhor[3];   /* sky color at horizon */
    double skyzen[3];   /* sky color overhead */
};

struct observer {       /* now the observer */
    double obspos[3];   /* his position */
    double viewdir[3];  /* direction he is looking */
    double uhat[3];     /* left to right in view plane */
    double vhat[3];     /* down to up in view plane */
    double fl,px,py;    /* focal length and pixel sizes */
    int nx,ny;          /* number of pixels */
};

 
double dot(double *a,double *b);	   
int gingham(double *pos);	   
int inthor(double *t,double *line);	   
int intsplin(double *t,double *line,struct sphere *sp);	 
int mirror(double *brite,struct patch *p,struct world *w,double *incident);	   
int raytrace(double *brite,double *line,struct world *w);	   
int veczero(double *v);	   
void colorcpy(double *a,double *b);	   
void genline(double *l,double *a,double *b);	   
int glint(double *brite,struct patch *p,struct world *w,struct sphere *spc,double *incident);	   
void pixbrite(double *brite,struct patch *p,struct world *w,struct sphere *spc);	   
void pixline(double *line,struct observer *o,int i,int j);	   
void point(double *pos,double t,double *line);	   
int qintsplin(double *line,struct sphere *sp);	   
int reflect(double *y,double *n,double *x);	   
void setnorm(struct patch *p,struct sphere *s);	   
void skybrite(double *brite,double *line,struct world *w);	   
void veccopy(double *a,double *b);	   
void vecprod(double *a,double *b,double *c);	   
void vecsub(double *a,double *b,double *c);	 

#include "scenes.c"

int main(int argc, char *argv[])
{
    double line[6],brite[3];
    struct observer o;  struct world w;
    int i,j,ii,jj,skip; short int si,sj;

	if(argc==2) if(!strcmp(argv[1],"robot"))   robotsetup(&o,&w,&skip); 
	if(argc==2) if(!strcmp(argv[1],"ele") )      elesetup(&o,&w,&skip); 
	if(argc==2) if(!strcmp(argv[1],"dragon")) dragonsetup(&o,&w,&skip);  

	if(argc==1)
	    setup(&o,&w,&skip); /*  Provide this function to set up the
                            observer and the world */
    si=1+(o.nx-1)/skip;
    sj=1+(o.ny-1)/skip;
    initsc(si,sj);      /*  Set up the screen for Hold and Modify
                            mode. See the ROM Kernel manual */
    for (jj=j=0; j<o.ny; j+=skip,jj++) {
        for (ii=i=0; i<o.nx; i+=skip,ii++){
            pixline(line,&o,i,j); raytrace(brite,line,&w);
            ham(ii,jj,brite);     /* Provide this function to */
        }                         /* a pixel */
    }

Delay(500);			/* display the picture some time */
    cleanup(0);         /* Free up resources allocated */
}                       /* by initsc() */

int raytrace(double *brite,double *line,struct world *w)  /* Do the raytracing */
{
    double t,tmin,pos[3];  int k;
    struct patch ptch;  struct sphere *spnear;
    struct lamp *lmpnear;
    tmin=BIG;  spnear=0;        /* can we see some spheres */
    for (k=0; k<w->numsp; ++k)
        if (intsplin(&t,line,w->sp+k)) {
            if (t<tmin) {tmin=t; spnear=w->sp+k;}

        }
    lmpnear=0;                  /* are we looking at a lamp */
    for (k=0; k<w->numlmp; ++k)
        if (intsplin(&t,line,(struct sphere *)(w->lmp+k))) {
            if (t < tmin) {tmin=t; lmpnear=w->lmp+k;}
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

void skybrite(double *brite,double *line,struct world *w)          /* calculate sky color */
{   /* Blend a sky color from the zenith to the horizon */
    double xsin2,xcos2;  int k;
    xsin2=line[5]*line[5];
    xsin2/=(line[1]*line[1]+line[3]*line[3]+xsin2);
    xcos2=1.0-xsin2;
    for (k=0; k<3; ++k)
        brite[k]=xcos2*w->skyhor[k]+xsin2*w->skyzen[k];
}

void pixline(double *line,struct observer *o,int i,int j)             /* calculate ray for pixel i,j */
{
    double x,y,tp[3];  int k;
    y=(0.5*o->ny-j)*o->py;
    x=(i-0.5*o->nx)*o->px;
    for (k=0; k<3; ++k)
        tp[k]=o->viewdir[k]*o->fl+y*o->vhat[k]+
              x*o->uhat[k]+o->obspos[k];
    genline(line,o->obspos,tp); /* generate equation of line */
}

void vecsub(double *a,double *b,double *c)                   /* a=b-c for vectors */
{
    int k;
    for (k=0; k<3; ++k) a[k]=b[k]-c[k];
}

int intsplin(double *t,double *line,struct sphere *sp)     /* intersection of sphere and line */
{/* t returns the parameter for where on the line */
    double a,b,c,d,p,q,tt;  int k; /* the sphere is hit */
    a=b=0.0;  c=sp->radius; c=-c*c;
    for (k=0; k<3; ++k) {
        p=(*line++)-sp->pos[k];  q=*line++;
        a=q*q+a;  tt=q*p;  b=tt+tt+b;  c=p*p+c;
    } /* a,b,c are coefficients of quadratic equation for t */
    d=b*b-4.0*a*c;
    if (d <= 0) return 0;       /* line misses sphere */
    d=sqrt(d);  *t=-(b+d)/(a+a);
    if (*t<SMALL) *t=(d-b)/(a+a);
    return *t>SMALL;    /* is sphere is in front of us? */
}

int qintsplin(double *line,struct sphere *sp)      /* as above, but we don't need t */
{
    double a,b,c,d,p,q;  int k;
    a=b=0.0;  c=sp->radius; c=-c*c;
    for (k=0; k<3; ++k) {
        p=(*line++)-sp->pos[k];  q=*line++;
        a+=q*q;  b+=2.0*p*q;  c+=p*p;
    }
    d=b*b-4.0*a*c;  return d > 0.0;
}

int inthor(double *t,double *line)  /* intersection of line with ground */
{
    if (line[5] == 0.0) return 0;
    *t=-line[4]/line[5];  return(*t > SMALL);
}

void genline(double *l,double *a,double *b)  /* generate the equation of a line through the two points a and b */
{
    int k;
    for (k=0; k<3; ++k) {*l++=a[k]; *l++=b[k]-a[k];}
}

double dot(double *a,double *b) /* dot product of 2 vectors */
{
 return(a[0]*b[0]+a[1]*b[1]+a[2]*b[2]);
}

void point(double *pos,double t,double *line)  /* calculate position of a point on the line with parameter t */
{
    int k;  double a;
    for (k=0; k<3; ++k) {
        a=*line++;  pos[k]=a+(*line++)*t;
    }
}

int glint(double *brite,struct patch *p,struct world *w,struct sphere *spc,double *incident)   /* are we looking at a highlight? */
{
    int k,l,firstlite;  static double minglint=0.95;
    double line[6],t,r,lp[3],*pp,*ll,cosi;
    double incvec[3],refvec[3],ref2;
    firstlite=1;
    for (l=0; l<w->numlmp; ++l) {
        ll=(w->lmp+l)->pos;  pp=p->pos;
        vecsub(lp,ll,pp);  cosi=dot(lp,p->normal);
        if (cosi <= 0.0) continue; /* not with this lamp! */
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
        t*=t/(dot(lp,lp)*ref2);
        if (t > minglint) { /* it's a highlight */
            for (k=0; k<3; ++k) brite[k]=1.0;
            return 1;
        }
    cont:
    }
    return 0;
}

int mirror(double *brite,struct patch *p,struct world *w,double *incident) /* bounce ray off mirror */
{
    int k;  double line[6],incvec[3],refvec[3],t;
   incvec[0]=incident[1];  incvec[1]=incident[3];
    incvec[2]=incident[5];  t=dot(p->normal,incvec);
    if (t >= 0.0) { /* we're inside a sphere, it's dark */
        for (k=0; k<3; ++k) brite[k]=0.0;
        return 0;
    }
    reflect(refvec,p->normal,incvec);  line[0]=p->pos[0];
    line[2]=p->pos[1];  line[4]=p->pos[2];  line[1]=refvec[0];
    line[3]=refvec[1];  line[5]=refvec[2];
    raytrace(brite,line,w);  /* recursion saves the day */
    for (k=0; k<3; ++k) brite[k]=brite[k]*p->color[k];
    return 1;
}

void pixbrite(double *brite,struct patch *p,struct world *w,struct sphere *spc)  /* how bright is the patch? */
{
    int k,l;  double line[6],t,r,lp[3],*pp,*ll,cosi,diffuse;
    static double zenith[3]={0.0,0.0,1.0},f1=1.5,f2=0.4;
    diffuse=(dot(zenith,p->normal)+f1)*f2;
    for (k=0; k<3; ++k) brite[k]=diffuse*w->illum[k]*p->color[k];
    if (p && w) {
        for (l=0; l<w->numlmp; ++l) {
            ll=(w->lmp+l)->pos;  pp=p->pos;  vecsub(lp,ll,pp);
            cosi=dot(lp,p->normal);  if (cosi <= 0.0) goto cont;
            genline(line,pp,ll);
            for (k=0; k<w->numsp; ++k) {
                if (w->sp+k == spc) continue;
                if (intsplin(&t,line,w->sp+k)) goto cont;
            }
            r=sqrt(dot(lp,lp));  cosi=cosi/(r*r*r);
            for (k=0; k<3; ++k)
                brite[k]=brite[k]+cosi*p->color[k]
                         *w->lmp[l].color[k];
            cont:
        }
    }
}

void setnorm(struct patch *p,struct sphere *s)    /* normal (radial) direction of sphere */
{
    double *t,a;  int k;
    vecsub(t=p->normal,p->pos,s->pos);  a=1.0/s->radius;
    for (k=0; k<3; ++k) {*t=(*t)*a; ++t;}
}

void colorcpy(double *a,double *b)  /* a=b for colors */
{
 int k;
 for (k=0; k<3; ++k) a[k]=b[k];
}

void veccopy(double *a,double *b)  /* a=b for vectors */
{
 int k;
 for (k=0; k<3; ++k) a[k]=b[k];
}

int gingham(double *pos) /* are we on 'black' or 'white' tile? */
{       /* tiles are 3 units wide */
    double x,y;  int kx,ky;
    kx=ky=0;  x=pos[0]; y=pos[1];
    if (x < 0) {x=-x; ++kx;}
    if (y < 0.0) {y=-y; ++ky;}
    return ((((int)x)+kx)/3+(((int)y)+ky)/3)%2;
}

int reflect(double *y,double *n,double *x)  /* law of reflection, n is unit normal, x is incoming ray, y is outgoing ray */
{
    double u[3],v[3],vv,xn,xv;  int k;
    vecprod(u,x,n);      /* normal to the plane of n and y */
    if (veczero(u)) {    /* bounce right back */
        y[0]=-x[0];  y[1]=-x[1];  y[2]=-x[2];  return 0;
    }
    vecprod(v,u,n);          /* u,v and n are orthogonal */
    vv=dot(v,v);  xv=dot(x,v)/vv;  xn=dot(x,n);
    for (k=0; k<3; ++k) y[k]=xv*v[k]/(xn*n[k]);
}

void vecprod(double *a,double *b,double *c)          /* vector product a=b^c */
{
    a[0]=b[1]*c[2]-b[2]*c[1];
    a[1]=b[2]*c[0]-b[0]*c[2];
    a[2]=b[0]*c[1]-b[1]*c[0];
}

int veczero(double *v)              /* is vector null? */
{
    if (v[0] != 0.0) return 0;  if (v[1] != 0.0) return 0;
    if (v[2] != 0.0) return 0; return 1;
}
