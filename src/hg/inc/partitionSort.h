/* support for partitioning genome annotation files into non-overlapping regions */

#ifndef PARTITIONSORT_H
#define PARTITIONSORT_H
#include "pipeline.h"

static struct pipeline *partitionSortOpenPipeline(char *inFile, int chromCol, int startCol,
                                                  int endCol, int parallel)
/* Open pipeline that sorts a file.  Columns are zero-based, parallel greater than one
 * enables parallel sort with this many cores. */
{
static char *zcatCmd[] = {"zcat", NULL};
static char *bzcatCmd[] = {"bzcat", NULL};

// reverse end allows finding non-overlapping
int iSort = 0;
char *sortCmd[6], chromSpec[64], startSpec[64], endSpec[64], parallelSpec[64];

sortCmd[iSort++] = "sort";
safef(chromSpec, sizeof(chromSpec), "-k%d,%d", chromCol+1, chromCol+1);
sortCmd[iSort++] = chromSpec;
safef(startSpec, sizeof(startSpec), "-k%d,%dn", startCol+1, startCol+1);
sortCmd[iSort++] = startSpec;
safef(endSpec, sizeof(endSpec), "-k%d,%dnr", endCol+1, endCol+1);
sortCmd[iSort++] = endSpec;
if (parallel > 1)
    {
    safef(parallelSpec, sizeof(parallelSpec), "--parallel=%d", parallel);
    sortCmd[iSort++] = parallelSpec;
    }
sortCmd[iSort] = NULL;
assert(iSort < ArraySize(sortCmd));

int iCmd = 0;
char **cmds[3];
if (endsWith(inFile, ".gz") || endsWith(inFile, ".Z"))
    cmds[iCmd++] = zcatCmd;
else if (endsWith(inFile, ".bz2"))
    cmds[iCmd++] = bzcatCmd;
cmds[iCmd++] = sortCmd;
cmds[iCmd++] = NULL;

return pipelineOpen(cmds, pipelineRead, inFile, NULL, 0);
}

#endif
