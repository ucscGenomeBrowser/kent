/* Parse a statistics file from autoDtd and return it as C data structure. */
/* This file is copyright 2005 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "elStat.h"

struct elStat *elStatLoadAll(char *fileName)
/* Read all elStats from file. */
{
struct elStat *list = NULL, *el = NULL;
struct attStat *att;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[6];
int wordCount;

for (;;)
   {
   wordCount = lineFileChop(lf, row);
   if (wordCount == 2)
       {
       AllocVar(el);
       el->name = cloneString(row[0]);
       el->count = lineFileNeedNum(lf, row, 1);
       slAddHead(&list, el);
       }
    else if (wordCount == 5)
       {
       if (el == NULL)
           errAbort("%s doesn't start with an element", fileName);
       if (!sameString(row[2], "none"))
	   {
	   AllocVar(att);
	   att->name = cloneString(row[0]);
	   att->maxLen = lineFileNeedNum(lf, row, 1);
	   att->type = cloneString(row[2]);
	   att->count = lineFileNeedNum(lf, row, 3);
	   att->uniqCount = lineFileNeedNum(lf, row, 4);
	   slAddTail(&el->attList, att);
	   }
       }
    else if (wordCount == 0)
       break;
    else
       errAbort("Don't understand line %d of %s", lf->lineIx, lf->fileName);
    }
slReverse(&list);
return list;
}

