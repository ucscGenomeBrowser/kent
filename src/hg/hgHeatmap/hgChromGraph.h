
/* hard-code for demo.
   the space for the CrhomGraph is hardcoded here chromGraphOffset=10    
*/

#include "memgfx.h"

#ifndef HGCHROMGRAPH_H
#define HGCHROMGRAPH_H


int chromGraphHeight();

double chromGraphMax(char* tableName);

double chromGraphMin(char* tableName);

int chromGraphMaxGapToFill(char* tableName);

Color chromGraphColor(char* tableName);

#endif /* HGCHROMGRAPH_H */
