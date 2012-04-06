#include "common.h" 
#include "jksql.h"
#include "output/mainTest.h"


int nextId()
{
static int id = 1000;
return ++id;
}

struct polygon *makeTriangle(int x, int y, int z)
/* Make triangle about center x,y,z */
{
struct polygon *tri;
struct point *pt;
int i;
AllocVar(tri);
tri->id = nextId();
tri->pointCount = 3;
AllocVar(pt);
strcpy(pt->acc, "acc1");
pt->pt.x = pt->x = x;
pt->pt.y = pt->y = y-100;
pt->z = z;
slAddTail(&tri->points, pt);
AllocVar(pt);
strcpy(pt->acc, "acc2");
pt->pt.x = pt->x = x-100;
pt->pt.y = pt->y = y+100;
pt->z = z;
slAddTail(&tri->points, pt);
AllocVar(pt);
strcpy(pt->acc, "acc3");
pt->pt.x = pt->x = x+100;
pt->pt.y = pt->y = y+100;
pt->z = z;
slAddTail(&tri->points, pt);
AllocArray(tri->persp, 3);
for (i=0, pt = tri->points; i < 3; ++i, pt = pt->next)
    {
    tri->persp[i].x = pt->x/2;
    tri->persp[i].y = pt->y/2;
    }
return tri;
}

void findBox(struct polyhedron *ph)
/* Fill in screen box based on max/min screen points. */
{
struct polygon *pg;
int minX = 0x3ffffff;
int minY = minX;
int maxX = -minX;
int maxY = -minY;

for (pg = ph->polygons; pg != NULL; pg = pg->next)
    {
    int i;
    for (i=0; i<pg->pointCount; ++i)
	{
	struct pt *pt = &pg->persp[i];
	int x = pt->x;
	int y = pt->y;
	if (x < minX) minX = x;
	if (x > maxX) maxX = x;
	if (y < minY) minY = y;
	if (y > maxY) maxY = y;
	}
    }
ph->screenBox[0].x = minX;
ph->screenBox[0].y = minY;
ph->screenBox[1].x = maxX;
ph->screenBox[1].y = maxY;
}

void testPolyhOut()
{
struct polyhedron *ph;
AllocVar(ph);
ph->id = nextId();
ph->names[0] = cloneString("Jim");
ph->names[1] = cloneString("Kent");
ph->polygonCount = 2;
slAddTail(&ph->polygons, makeTriangle(0, 0, 0));
slAddTail(&ph->polygons, makeTriangle(0, 0, 100));
findBox(ph);
polyhedronCommaOut(ph, stdout);
printf("\n");
polyhedronFree(&ph);
}

void testPolyOut()
{
struct polygon *tri;
struct point *pt;
int i;

AllocVar(tri);
tri->id = 21060;
tri->pointCount = 4;
for (i=0; i<tri->pointCount; ++i)
    {
    AllocVar(pt);
    sprintf(pt->acc, "point-%c", 'A'+i);
    pt->x = 100 + i;
    pt->y = 200 + i;
    pt->z = 300 + i;
    slAddTail(&tri->points, pt);
    }
AllocArray(tri->persp,tri->pointCount);
for (i=0; i<tri->pointCount; ++i)
    {
    tri->persp[i].x = 50+i;
    tri->persp[i].y = 100+i;
    }
polygonCommaOut(tri, stdout);
printf("\n");
}

void testPolyIn()
{
char *in = cloneString(
  "21060,4,"
  "{{\"p1\",100,200,300,{100,200,},},"
   "{\"p2\",101,201,301,{101,201,},},"
   "{\"p3\",102,202,302,{102,202,},},"
   "{\"p4\",103,203,303,{103,203,},},},"
  "{{100,200,},{101,201,},{102,202,},{103,203,},},");
struct polygon *tri = polygonCommaIn(&in, NULL);
struct point *pt;
printf("id = %d\n", tri->id);
printf("pointCount = %d\n", tri->pointCount);
for (pt = tri->points; pt != NULL; pt = pt->next)
    printf("(%d %d %d) ", pt->x, pt->y, pt->z);
printf("\n");
}

void testPolyhIn()
{
char *in = 
    "1001,{\"Jim\",\"Kent\",},2,"
	"{"
	    "{1002,3,"
	        "{"
	        "{\"a1\",0,-100,0,{0,-50,},},"
		"{\"a2\",-100,100,0,{-50,50,},},"
		"{\"a3\",100,100,0,{50,50,},},"
		"},"
	    "{{0,-50,},{-50,50,},{50,50,},},"
	    "},"
	    "{1003,3,"
	        "{"
		"{\"b1\",0,-100,100,{-50,50,},},"
		"{\"b2\",-100,100,100,{-50,50,},},"
		"{\"b3\",100,100,100,{50,50,},},"
		"},"
	    "{{-50,50,},{-50,50,},{50,50,},},"
	    "},"
	"},"
	"{{-50,-50,},{50,50,},},";

struct polyhedron *ph;
struct polygon *poly;

in = cloneString(in);
uglyf("testPolyhIn - in = %s\n", in);
ph = polyhedronCommaIn(&in,NULL);
printf("polyhedron id = %d names = %s %s polygonCount = %d\n", 
    ph->id, ph->names[0], ph->names[1], ph->polygonCount);
for (poly = ph->polygons; poly != NULL; poly = poly->next)
    {
    struct point *pt;
    printf("  polygon id = %d\n", poly->id);
    printf("  pointCount = %d\n", poly->pointCount);
    for (pt = poly->points; pt != NULL; pt = pt->next)
	printf("   (%d %d %d) ", pt->x, pt->y, pt->z);
    printf("\n");
    }
}

int main()
{
testPolyOut();
uglyf("Ok1\n");
testPolyIn();
uglyf("Ok2\n");
testPolyhOut();
uglyf("Ok3\n");
testPolyhIn();
uglyf("Ok4\n");
return 0;
}
