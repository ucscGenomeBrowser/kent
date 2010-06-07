/* ooUtils - utilities to help manage building o+o directories and
 * the like. */
#include "common.h"
#include "portable.h"
#include "ooUtils.h"

static char const rcsid[] = "$Id: ooUtils.c,v 1.3 2006/03/09 18:26:58 angie Exp $";

void ooToAllContigs(char *ooDir, void (*doContig)(char *dir, char *chrom, char *contig))
/* Apply "doContig" to all contig subdirectories of ooDir. */
{
struct fileInfo *chromList = NULL, *contigList = NULL, *chromEl, *contigEl;
char chromPath[512], contigPath[512];

chromList = listDirX(ooDir, "*", FALSE);
for (chromEl = chromList; chromEl != NULL; chromEl = chromEl->next)
    {
    if (!chromEl->isDir) continue;
    if (strlen(chromEl->name) > 2) continue;
    safef(chromPath, sizeof(chromPath), "%s/%s", ooDir, chromEl->name);
    contigList = listDirX(chromPath, "ctg*", FALSE);
    for (contigEl = contigList; contigEl != NULL; contigEl = contigEl->next)
        {
	if (!contigEl->isDir) continue;
	safef(contigPath, sizeof(contigPath), "%s/%s",
	      chromPath, contigEl->name);
	(*doContig)(contigPath, chromEl->name, contigEl->name);
	}
    slFreeList(&contigList);
    }
slFreeList(&chromList);
}


