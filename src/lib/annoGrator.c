/* annoGrator -- object framework for integrating genomic annotations from disparate sources */

#include "common.h"
#include "annoGrator.h"

void annoGratorQueryExecute(struct annoGratorQuery *query)
/* For each annoRow from query->primarySource, invoke integrators and pass their annoRows
 * to formatters. */
{
printf("hello world!\n");
}

void annoGratorQueryFree(struct annoGratorQuery **pQuery)
/* Close and free all inputs and outputs; free self. */
{
printf("goodbye world!\n");
}
