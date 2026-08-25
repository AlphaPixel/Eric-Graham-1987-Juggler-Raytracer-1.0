/*      DATPARSE.C
        Parser for the original raytracer .dat scene files.
*/

#define _CRT_SECURE_NO_WARNINGS

#include "rt.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

struct parser {
    char *buf;
    char *p;
};

struct sphere_list {
    struct sphere *v;
    int n;
    int cap;
};

static void skipspace(struct parser *ps)
{
    while (*ps->p && isspace((unsigned char)*ps->p)) ++ps->p;
}

static int want(struct parser *ps,int ch)
{
    skipspace(ps);
    if (*ps->p != ch) return 0;
    ++ps->p;
    return 1;
}

static int number(struct parser *ps,double *v)
{
    char *end;
    skipspace(ps);
    *v=strtod(ps->p,&end);
    if (end == ps->p) return 0;
    ps->p=end;
    return 1;
}

static int integer(struct parser *ps,int *v)
{
    char *end;
    long n;
    skipspace(ps);
    n=strtol(ps->p,&end,10);
    if (end == ps->p) return 0;
    *v=(int)n;
    ps->p=end;
    return 1;
}

static int vec3(struct parser *ps,double v[3],int left,int right)
{
    if (!want(ps,left)) return 0;
    if (!number(ps,&v[0])) return 0;
    if (!want(ps,',')) return 0;
    if (!number(ps,&v[1])) return 0;
    if (!want(ps,',')) return 0;
    if (!number(ps,&v[2])) return 0;
    if (!want(ps,right)) return 0;
    return 1;
}

static int vec2(struct parser *ps,double v[2])
{
    if (!want(ps,'[')) return 0;
    if (!number(ps,&v[0])) return 0;
    if (!want(ps,',')) return 0;
    if (!number(ps,&v[1])) return 0;
    if (!want(ps,']')) return 0;
    return 1;
}

static int spherepoint(struct parser *ps,double pos[3],double *radius)
{
    if (!vec3(ps,pos,'(',')')) return 0;
    if (!want(ps,':')) return 0;
    if (!number(ps,radius)) return 0;
    return 1;
}

static int addsphere(struct sphere_list *list,struct sphere *sp)
{
    struct sphere *n;

    if (list->n == list->cap) {
        list->cap=list->cap ? list->cap*2 : 32;
        n=(struct sphere *)realloc(list->v,sizeof(struct sphere)*list->cap);
        if (!n) return 0;
        list->v=n;
    }
    list->v[list->n++]=*sp;
    return 1;
}

static int addseg(struct sphere_list *list,struct sphere *base,
                  double lastpos[3],double lastradius,
                  double nextpos[3],double nextradius,int nseg)
{
    int i,k;
    double t;
    struct sphere sp;

    if (nseg <= 0) return 0;
    sp=*base;
    for (i=1; i<=nseg; ++i) {
        t=(double)i/(double)nseg;
        for (k=0; k<3; ++k)
            sp.pos[k]=lastpos[k]+(nextpos[k]-lastpos[k])*t;
        sp.radius=lastradius+(nextradius-lastradius)*t;
        if (!addsphere(list,&sp)) return 0;
    }
    return 1;
}

static int parseobject(struct parser *ps,struct sphere_list *list)
{
    int k,nseg;
    double lastpos[3],nextpos[3],lastradius,nextradius;
    struct sphere base;

    if (!vec3(ps,base.color,'<','>')) return 0;
    if (!integer(ps,&base.type)) return 0;
    if (!spherepoint(ps,lastpos,&lastradius)) return 0;
    for (k=0; k<3; ++k) base.pos[k]=lastpos[k];
    base.radius=lastradius;
    if (!addsphere(list,&base)) return 0;

    for (;;) {
        skipspace(ps);
        if (*ps->p == ';') {
            ++ps->p;
            return 1;
        }
        if (!integer(ps,&nseg)) return 0;
        if (!spherepoint(ps,nextpos,&nextradius)) return 0;
        if (!addseg(list,&base,lastpos,lastradius,
                    nextpos,nextradius,nseg)) return 0;
        for (k=0; k<3; ++k) lastpos[k]=nextpos[k];
        lastradius=nextradius;
    }
}

static int readfile(struct parser *ps,const char *name)
{
    FILE *f;
    long len;

    f=fopen(name,"rb");
    if (!f) return 0;
    if (fseek(f,0,SEEK_END) != 0) { fclose(f); return 0; }
    len=ftell(f);
    if (len < 0) { fclose(f); return 0; }
    if (fseek(f,0,SEEK_SET) != 0) { fclose(f); return 0; }
    ps->buf=(char *)malloc((size_t)len+1);
    if (!ps->buf) { fclose(f); return 0; }
    if (fread(ps->buf,1,(size_t)len,f) != (size_t)len) {
        fclose(f);
        free(ps->buf);
        ps->buf=0;
        return 0;
    }
    fclose(f);
    ps->buf[len]=0;
    ps->p=ps->buf;
    return 1;
}

static void setupobserver(struct observer *o,double alt,double az,double fl)
{
    double degtorad;

    degtorad=0.0174533;
    o->nx=320; o->ny=200;
    o->px=1.0/o->nx; o->py=0.75/o->ny;
    o->fl=0.028*fl;

    alt*=degtorad;
    az*=degtorad;

    o->viewdir[0]=cos(az)*cos(alt);
    o->viewdir[1]=sin(az)*cos(alt);
    o->viewdir[2]=sin(alt);

    o->uhat[0]=sin(az);
    o->uhat[1]=-cos(az);
    o->uhat[2]=0.0;

    o->vhat[0]=-cos(az)*sin(alt);
    o->vhat[1]=-sin(az)*sin(alt);
    o->vhat[2]=cos(alt);
}

static void exposelamps(struct world *w)
{
    double t,r,tp[3],lampfac;
    int i,j,k;

    lampfac=BIG;
    for (i=0; i<w->numsp; ++i)
        for (j=0; j<w->numlmp; ++j) {
            vecsub(tp,w->sp[i].pos,w->lmp[j].pos);
            r=sqrt(dot(tp,tp));
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

int setupfromdat(struct observer *o,struct world *w,int *skip,const char *datfile)
{
    struct parser ps;
    struct sphere_list spheres;
    double angles[2],fl;
    int i,k;

    ps.buf=0;
    spheres.v=0;
    spheres.n=0;
    spheres.cap=0;
    w->numsp=0;
    w->sp=0;
    w->numlmp=0;
    w->lmp=0;

    if (!readfile(&ps,datfile)) {
        printf("\nUnable to read %s",datfile);
        return 0;
    }

    if (!vec3(&ps,o->obspos,'(',')') ||
        !vec2(&ps,angles) ||
        !number(&ps,&fl)) goto bad;

    setupobserver(o,angles[0],angles[1],fl);
    *skip=1;

    for (;;) {
        skipspace(&ps);
        if (*(ps.p) == ';') {
            ++(ps.p);
            break;
        }
        if (!parseobject(&ps,&spheres)) goto bad;
    }

    w->numsp=spheres.n;
    w->sp=spheres.v;
    spheres.v=0;

    if (!integer(&ps,&w->numlmp)) goto bad;
    w->lmp=(struct lamp *)malloc(sizeof(struct lamp)*w->numlmp);
    if (!w->lmp) goto bad;
    for (i=0; i<w->numlmp; ++i) {
        if (!spherepoint(&ps,w->lmp[i].pos,&w->lmp[i].radius) ||
            !vec3(&ps,w->lmp[i].color,'<','>')) goto bad;
    }

    if (!vec3(&ps,w->horizon[0].color,'<','>') ||
        !vec3(&ps,w->horizon[1].color,'<','>') ||
        !vec3(&ps,w->illum,'<','>') ||
        !vec3(&ps,w->skyzen,'<','>') ||
        !vec3(&ps,w->skyhor,'<','>')) goto bad;

    for (i=0; i<2; ++i) {
        w->horizon[i].normal[0]=0.0;
        w->horizon[i].normal[1]=0.0;
        w->horizon[i].normal[2]=1.0;
        for (k=0; k<3; ++k) w->horizon[i].pos[k]=0.0;
    }

    exposelamps(w);
    free(ps.buf);
    return 1;

bad:
    printf("\nUnable to parse %s near %.40s",datfile,ps.p);
    free(ps.buf);
    free(spheres.v);
    if (w->sp) free(w->sp);
    if (w->lmp) free(w->lmp);
    w->sp=0;
    w->lmp=0;
    return 0;
}
