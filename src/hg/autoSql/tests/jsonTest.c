/* test of the JSON output code */

#include <stdio.h>
#include "output/doc.h"
#include "output/doc2.h"

#include "common.h"
#include "errAbort.h"
#include "linefile.h"
#include "obscure.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "asParse.h"

#define NUMPOINTS 3
#define NUMFACES 2 

int main()
{
   int i, j;
   FILE* fp = stdout;
   struct polyhedron* ph;
   struct face* f;
   struct point* pt;
   struct addressBook* ab;
   struct symbolCols* sc;


   /* init the polyhedron */
   AllocVar(ph);
   ph->faceCount = 0;
   ph->pointCount = 0;
   AllocArray(ph->points,NUMPOINTS); 
   ph->faces = NULL;

   /* add faces to polyhedron */
   for (j = 0; j < NUMFACES; j++)
      { 
      /* init the face */
      AllocVar(f);
      AllocArray(f->points,NUMPOINTS);
      f->next = NULL;
      f->color.red = (unsigned) j;
      f->color.green = (unsigned)j+1;
      f->color.blue = (unsigned)j+2;
      f->pointCount = 0;
      for (i = 0; i < NUMPOINTS; i++)
          {
          f->points[i] = j + i;
          f->pointCount++;
          } 

      /* link the new face */
      if (ph->faces == NULL)
          ph->faces = f;
      else
          slSafeAddHead(&(ph->faces),f);
      ph->faceCount++;

      /* output the face */
      faceJsonOutput(f,fp);
      fprintf(fp,"\n\n");
      }
   slReverse(&(ph->faces));

   /* add points to polyhedron */
   pt = ph->points;
   for (j = 0; j < NUMPOINTS; j++)
      {
      /* init the point */
      pt->x = j;
      pt->y = 1.0 + j;
      pt->z = 2.0 + j;
      ph->pointCount++;

      /* print the point */
      pointJsonOutput(pt,fp);
      fprintf(fp,"\n\n");

      pt++;
      }

   /* print the polyhedron */
   polyhedronJsonOutput(ph,fp);
   fprintf(fp,"\n\n");

   /* init the addressBook */
   AllocVar(ab);
   ab->next = NULL;
   ab->name = "Joe Bob";
   ab->address = "Some Street";
   ab->city = "Any Town";
   ab->zipCode = 12345;
   ab->state[0] = 'C';
   ab->state[1] = 'A';
   ab->state[2] = '\0';

   /* output addressBook */
   addressBookJsonOutput(ab, fp);
   fprintf(fp,"\n\n");

   /* init symbolCols */
   AllocVar(sc);
   sc->next = NULL;
   sc->id = 12345;
   sc->sex = symbolColsMale;
   sc->skills = symbolColsCProg;

   /* output symbolCols */
   symbolColsJsonOutput(sc, fp);
   fprintf(fp,"\n\n");

   return 0;
}
