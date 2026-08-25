/* Code to load RT scenes maded with data from robot.dat ele.dat dragon.dat	*/
/* Alain Thellier - Paris - France - 2014 - Free source				*/ 

#include <proto/exec.h>

int spnum=0;
/*=================================================================*/
void finishsetup(struct observer *o,struct world *w,int *skip)
{
 int i,j,k;
 double t,r,tp[3],lampfac;

 *skip=1;
 o->nx=320; o->ny=200;
 o->px=1.00/o->nx;
 o->py=0.75/o->ny;

 w->horizon[0].normal[0]=0.0;
 w->horizon[0].normal[1]=0.0;
 w->horizon[0].normal[2]=1.0;
 w->horizon[1].normal[0]=0.0;
 w->horizon[1].normal[1]=0.0;
 w->horizon[1].normal[2]=1.0;

 lampfac=BIG;                   /* modify the lamp brightness so as to */
 for (i=0; i<w->numsp; ++i)     /* get the right exposure              */
        for (j=0; j<w->numlmp; ++j)
                {vecsub(tp,w->sp[i].pos,w->lmp[j].pos);
                 r=sqrt(dot(tp,tp));
                 r-=w->sp[i].radius;
                 for (k=0; k<3; ++k)
                        {t=w->sp[i].color[k]*w->lmp[j].color[k]/(r*r);
                         if (t == 0.0) continue;
                         t=(1.0-w->sp[i].color[k]*w->illum[k])/t;
                         if (t<lampfac) lampfac=t;
                        }
                }

 for (j=0; j<w->numlmp; ++j)
        for (k=0; k<3; ++k)
                w->lmp[j].color[k]*=lampfac;

printf("Scene got %ld spheres",spnum);
}
/*=================================================================*/
void SetVar3(double *a,float f0,float f1, float f2)
{
a[0]=f0;
a[1]=f1;
a[2]=f2;
}
/*=================================================================*/
struct sphere *Sphere(struct sphere *sp,float r,float g,float b,int type,float x,float y,float z,float radius)
{
SetVar3(sp->color,r,g,b);
SetVar3(sp->pos,x,y,z);
sp->radius=radius;
sp->type=type;
sp=sp+1;
spnum++;
return(sp);
}
/*=================================================================*/
struct sphere *MultiSphere(struct sphere *sp,int numsp,float x,float y,float z,float r)
{
int i;
struct sphere *sp0=sp-1;
float x0,y0,z0,r0;
float xs,ys,zs,rs;


x0=sp0->pos[0];
y0=sp0->pos[1];
z0=sp0->pos[2];
r0=sp0->radius;

xs=(x-x0)/((float)numsp);
ys=(y-y0)/((float)numsp);
zs=(z-z0)/((float)numsp);
rs=(r-r0)/((float)numsp);


for (i=0; i<numsp; ++i)
	{
	x=x0+xs*((float)i);
	y=y0+ys*((float)i);
	z=z0+zs*((float)i);
	r=r0+rs*((float)i);
	sp=Sphere(sp,sp0->color[0],sp0->color[1],sp0->color[2],sp0->type,x,y,z,r);
	}

return(sp);
}
/*=================================================================*/
void Lamp(struct lamp *lmp,float x,float y,float z,float radius,float r,float g,float b)
{
SetVar3(lmp->color,r,g,b);
SetVar3(lmp->pos,x,y,z);
lmp->radius=radius;
}
/*=================================================================*/
void LampNum(struct world *w,int numlmp)
{
 w->numlmp=numlmp;
 w->lmp=(struct lamp *)malloc(sizeof(struct lamp)*w->numlmp);
 if (!w->lmp)
        {printf("\nUnable to allocate lamp memory"); cleanup("lamp alloc");}
}
/*=================================================================*/
void SphereNum(struct world *w,int numsp)
{
 w->numsp=numsp;
 w->sp=(struct sphere *)malloc(sizeof(struct sphere)*w->numsp);
 if (!w->sp)
        {printf("\nUnable to allocate memory"); cleanup(0);}
}
/*=================================================================*/
void SetAltAz(struct observer *o,float alt,float az)
{
float	degtorad=0.0174533;
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
/*=================================================================*/
void SetFocal(struct observer *o,float focal)
{
 o->fl=focal;
 o->fl=o->fl*0.028;
}
/*=================================================================*/
void robotsetup(struct observer *o,struct world *w,int *skip)
{
struct lamp *lmp;
struct sphere *sp;

SetVar3(o->obspos,-10,-4,5.5);
SetAltAz(o,-10,20);
SetFocal(o,35);

SphereNum(w,70);
sp=w->sp;
sp=Sphere(sp,0.9,0.9,0.9, 2  ,-0.9,-2.1,5.3,0.6);
sp=Sphere(sp,0.9,0.9,0.9, 2  ,-1.1,1.9,5.9,0.6);
sp=Sphere(sp,0.9,0.9,0.9, 2  ,-0.4,-1.2,6.8,0.6);
sp=Sphere(sp,1,0.7,0.7,  1  ,0,0,6.1,0.5);
sp=Sphere(sp,0.2,0.1,0.1, 1  ,0.02,0,6.12,0.5);
sp=Sphere(sp,0.1,0.1,1.0, 1  ,-0.4,0.2,6.1,0.15);
sp=Sphere(sp,0.1,0.1,1.0, 1  ,-0.4,-0.2,6.1,0.15);
sp=Sphere(sp,1,0.7,0.7,  1  ,0,0,5.5,0.2);
sp=Sphere(sp,1,0.1,0.1,  1  ,0,0,4.6,0.8);
	sp=MultiSphere(sp,5 ,0,0,3.3,0.6);
sp=Sphere(sp,1,0.7,0.7,  1  ,0,0.6,2.9,0.2);
	sp=MultiSphere(sp,6 ,-0.6,0.6,1.6,0.2);
	sp=MultiSphere(sp,7 ,-0.4,0.6,0,0.1);
sp=Sphere(sp,1,0.7,0.7,  1  ,0,-0.6,2.9,0.2);
	sp=MultiSphere(sp,6 ,0.2,-0.6,1.6,0.2);
	sp=MultiSphere(sp,7 ,0.4,-0.6,0,0.1);
sp=Sphere(sp,1,0.7,0.7,  1  ,0,-0.7,5.1,0.2);
	sp=MultiSphere(sp,6 ,-0.2,-1.2,4.2,0.2);
	sp=MultiSphere(sp,7 ,-1.1,-2.0,4.1,0.1);
sp=Sphere(sp,1,0.7,0.7,  1  ,0,0.7,5.1,0.2);
	sp=MultiSphere(sp,6 ,-0.2,1.2,4.2,0.2);
	sp=MultiSphere(sp,7 ,-1.0,1.9,4.8,0.1);


LampNum(w,1);
lmp=w->lmp;
Lamp(lmp,-100,50,150,15,1,1,1);


SetVar3(w->horizon[0].color,1.5,1.5,0);
SetVar3(w->horizon[1].color,0,1.5,0);
SetVar3(w->illum,0.25,0.25,0.25);
SetVar3(w->skyhor,0.1,0.1,1);
SetVar3(w->skyzen,0.7,0.7,1);

finishsetup(o,w,skip);
}
/*=================================================================*/
void elesetup(struct observer *o,struct world *w,int *skip)
{
struct lamp *lmp;
struct sphere *sp;

SetVar3(o->obspos,0,-22,3);
SetAltAz(o,13,61);
SetFocal(o,28);

SphereNum(w,101);
sp=w->sp;

sp=Sphere(sp,0.7,0.6,0.7,0,1,-5,2,0.2);
	sp=MultiSphere(sp,6,1.7,-4.3,2.8,0.3);
	sp=MultiSphere(sp,5,2.0,-4,4.0,0.4);
	sp=MultiSphere(sp,5,2.3,-3.7,5.5,0.5);
	sp=MultiSphere(sp,5,2.7,-3.3,7.0,0.55);
	sp=MultiSphere(sp,5,3.0,-3.0,8.5,0.6);
	sp=MultiSphere(sp,5,4.0,-2.0,10.0,0.65);

sp=Sphere(sp,0.7,0.6,0.7,0,6.0,0,11.0,3.0);
sp=Sphere(sp,1,0,0,1,3.0,0.0,12.0,0.5);
sp=Sphere(sp,1,0,0,1,6.0,-3.0,12.0,0.5);

sp=Sphere(sp,0.7,0.6,0.7,0,11,0,8,3.5);
	sp=MultiSphere(sp,4,18,0,7.2,3.5);
	sp=MultiSphere(sp,4,25,0,8,3.5);

sp=Sphere(sp,0.7,0.6,0.7,0,28,0,10,0.2);
	sp=MultiSphere(sp,4,29,0,9.8,0.2);
	sp=MultiSphere(sp,4,30,0,9.0,0.2);
	sp=MultiSphere(sp,4,30,0,8.0,0.15);

sp=Sphere(sp,0.7,0.6,0.7,0,11,3,6,1);
	sp=MultiSphere(sp,5,9,4,3,1);
	sp=MultiSphere(sp,5,9,4,0,1);

sp=Sphere(sp,0.7,0.6,0.7,0,11,-3,6,1);
	sp=MultiSphere(sp,5,12,-4,3,1);
	sp=MultiSphere(sp,5,13,-4,0,1);

sp=Sphere(sp,0.7,0.6,0.7,0,25,3,6,1);
	sp=MultiSphere(sp,5,26,4,3,1);
	sp=MultiSphere(sp,5,27,4,0,1);

sp=Sphere(sp,0.7,0.6,0.7,0,25,-3,6,1);
	sp=MultiSphere(sp,5,23,-4,3,1);
	sp=MultiSphere(sp,5,23,-4,0,1);


LampNum(w,1);
lmp=w->lmp;
Lamp(lmp,-15,-50,40,15,1,1,1);

SetVar3(w->horizon[0].color,1.5,1.0,0);
SetVar3(w->horizon[1].color,1.5,1.0,0);
SetVar3(w->illum,0.25,0.25,0.25);
SetVar3(w->skyhor,0.1,0.1,1);
SetVar3(w->skyzen,0.7,0.7,1);

finishsetup(o,w,skip);
}
/*=================================================================*/
void dragonsetup(struct observer *o,struct world *w,int *skip)
{
struct lamp *lmp;
struct sphere *sp;

SetVar3(o->obspos,-22,-44,11);
SetAltAz(o,9,61);
SetFocal(o,28);

SphereNum(w,239);
sp=w->sp;

sp=Sphere(sp,1.0,0.7,0.7,1,26,6,15,15);
sp=Sphere(sp,0.2,1.0,0.2,0,-5,4.5,16,0.5);
	sp=MultiSphere(sp,10,-8,3,14,0.7);
	sp=MultiSphere(sp,5,-9.2,1,13.4,0.8);
	sp=MultiSphere(sp,5,-10,-2.5,13,0.9);
	sp=MultiSphere(sp,5,-9,-6,12.5,1.0);
	sp=MultiSphere(sp,5,-6,-9,12.5,1.1);
	sp=MultiSphere(sp,5,-2,-10.5,14,1.2);
	sp=MultiSphere(sp,5,3,-10,16,1.4);
	sp=MultiSphere(sp,5,9,-8.5,17.5,1.6);
	sp=MultiSphere(sp,4,13.5,-6,17,1.8);
	sp=MultiSphere(sp,4,15.5,-5,14.8,1.9);
	sp=MultiSphere(sp,3,16,-4,12,2.0);
	sp=MultiSphere(sp,3,14.9,-2.75,10,2.15);
	sp=MultiSphere(sp,3,13,-1.5,8,2.3);
	sp=MultiSphere(sp,3,7.5,0,6,3);
	sp=MultiSphere(sp,2,2,0,8,4.2);
	sp=MultiSphere(sp,2,0,0,12,6);
	sp=MultiSphere(sp,1,0,0,18,5);
	sp=MultiSphere(sp,1,0,0,23,3.5);
	sp=MultiSphere(sp,1,0,0,28,2.5);
	sp=MultiSphere(sp,1,-0.2,0,31,2);
	sp=MultiSphere(sp,1,-0.5,0,34,1.9);
	sp=MultiSphere(sp,1,-0.7,0,37.5,1.8);
	sp=MultiSphere(sp,1,-1.2,0,39,1.7);
	sp=MultiSphere(sp,1,-2.8,0,39.5,1.6);
	sp=MultiSphere(sp,1,-5,-0.5,38.5,1.4);
	sp=MultiSphere(sp,1,-6.5,-2,36.5,1.2);
sp=Sphere(sp,0.2,1.0,0.2,0,-1,-3.2,23,1);
	sp=MultiSphere(sp,8,-2,-4.4,18,0.9);
	sp=MultiSphere(sp,8,-8,-5,21,0.7);
sp=Sphere(sp,0.2,1.0,0.2,0,-1,3.2,23,1);
	sp=MultiSphere(sp,8,-3,4.4,20,0.9);
	sp=MultiSphere(sp,8,-8,5,19,0.7);
sp=Sphere(sp,0.2,1.0,0.2,0,-9.7,-2.2,34.5,2);
	sp=MultiSphere(sp,5,-5.9,-5.4,34.5,2);
sp=Sphere(sp,1.0,0,0,1,-11.3,-2,34,1.5);
sp=Sphere(sp,1.0,0,0,1,-5.4,-6.8,34,1.5);
sp=Sphere(sp,0.2,1.0,0.2,0,-10.2,-4.3,32.5,1);
	sp=MultiSphere(sp,5,-15.1,-11.3,24.5,0.8);
sp=Sphere(sp,0.2,1.0,0.2,0,-9.5,-4.9,32.5,1);
	sp=MultiSphere(sp,5,-15.0,-11.9,24.5,0.8);
sp=Sphere(sp,0.2,1.0,0.2,0,-8.7,-5.7,32.5,1);
	sp=MultiSphere(sp,5,-14.4,-12.4,24.5,0.8);
sp=Sphere(sp,0.2,1.0,0.2,0,-7.9,-6.3,32.5,1);
	sp=MultiSphere(sp,5,-13.8,-12.4,24.5,0.8);


/* problem somewhere here :-( */
sp=Sphere(sp,0.2,0.2,1.0,1,-15.3,-12.8,24,0.5);
	sp=MultiSphere(sp,7,-17,-13.6,22.5,0.5);
	sp=MultiSphere(sp,7,-17.9,-14.6,21.5,0.5);
	sp=MultiSphere(sp,7,-18.3,-15.6,20.5,0.3);
sp=Sphere(sp,0.2,0.2,1.0,1,-17.9,-14.6,21.5,0.5);
	sp=MultiSphere(sp,7,-19,-15,20.5,0.3);



sp=Sphere(sp,0.2,1.0,0.2,0,2,-4,10,3);
	sp=MultiSphere(sp,5,7,-6,4,2);
	sp=MultiSphere(sp,7,5,-6,0,1);
sp=Sphere(sp,0.2,1.0,0.2,0,4,-6,0.5,0.5);
	sp=MultiSphere(sp,5,-1,-6,0.3,0.3);
sp=Sphere(sp,0.2,1.0,0.2,0,4,-6,0.5,0.5);
	sp=MultiSphere(sp,5,-1,-4,0.3,0.3);
sp=Sphere(sp,0.2,1.0,0.2,0,4,-6,0.5,0.5);
	sp=MultiSphere(sp,5,-1,-8,0.3,0.3);
sp=Sphere(sp,0.2,1.0,0.2,0,2,4,10,3);
	sp=MultiSphere(sp,5,3,6,4,2);
	sp=MultiSphere(sp,7,2,6,0,1);
sp=Sphere(sp,0.2,1.0,0.2,0,1,6,0.5,0.5);
	sp=MultiSphere(sp,5,-4,6,0.3,0.3);
sp=Sphere(sp,0.2,1.0,0.2,0,1,6,0.5,0.5);
	sp=MultiSphere(sp,5,-4,4,0.3,0.3);
sp=Sphere(sp,0.2,1.0,0.2,0,1,6,0.5,0.5);
	sp=MultiSphere(sp,5,-4,8,0.3,0.3);

LampNum(w,1);
lmp=w->lmp;
Lamp(lmp,-45,-150,120,15,1,1,1);

SetVar3(w->horizon[0].color,1.5,1.0,0);
SetVar3(w->horizon[1].color,1.5,1.0,0);
SetVar3(w->illum,0.25,0.25,0.25);
SetVar3(w->skyhor,0.1,0.1,1);
SetVar3(w->skyzen,0.7,0.7,1);

finishsetup(o,w,skip);
}
/*=================================================================*/




