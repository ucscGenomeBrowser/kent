/* writeCluster - save a cluster to file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "ggMrnaAli.h"
#include "geneGraph.h"

static char const rcsid[] = "$Id: writeCluster.c,v 1.1 2007/01/27 04:15:18 kent Exp $";

static void writeMa(struct ggMrnaAli *ma, FILE *f)
{
fprintf(f, "%s,", ma->qName);
fprintf(f, "%s,%d,%d,%s,%d", ma->tName, ma->tStart, ma->tEnd, ma->strand, ma->blockCount);
}

void writeCluster(struct ggMrnaCluster *cluster, FILE *f)
/* Write out a cluster to file. */
{
struct maRef *mr;
fprintf(f, "%s\t%d\t%d\t", cluster->tName, cluster->tStart, cluster->tEnd);
fprintf(f, "%d\t", slCount(cluster->refList));
for (mr = cluster->refList; mr != NULL; mr = mr->next)
    {
    fprintf(f, "{");
    writeMa(mr->ma, f);
    fprintf(f, "}");
    };
fprintf(f, "\n");
}
