/* ooUtils - utilities to help manage building o+o directories and
 * the like. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "portable.h"
#include "ooUtils.h"


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


