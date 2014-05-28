/* asCompare - Compare .as autoSql files. */

/* Copyright (C) 2012 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "options.h"
#include "asParse.h"
#include "basicBed.h"
#include "errCatch.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "asCompare - Compare one given .as (AutoSql) file to one or more .as files, outputting the name and number of matching columns.\n"
  "usage:\n"
  "   asCompare compareTo.as compare.as [...]\n"
  "\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};


void asCompare(char *compareTo, char **compareAsFiles)
/* asCompare - Compare one autoSql files agaist many. */
{
struct asObject *asStandard = asParseFile(compareTo);
int numStandardCols = slCount(asStandard->columnList);
for (; *compareAsFiles; ++compareAsFiles)
    {
    struct asObject *asThis = NULL;
    int same = 0;
    /* protect against errAbort */
    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
	{
	asThis = asParseFile(*compareAsFiles);
	int numThisCols = slCount(asThis->columnList);
    	int numColumnsToCheck = min(numStandardCols,numThisCols);
	asCompareObjs("compareTo", asStandard, "this", asThis, numColumnsToCheck, &same, FALSE);
	}
    errCatchEnd(errCatch);
    if (errCatch->gotError)
	{
	warn(errCatch->message->string);
	}
    errCatchFree(&errCatch);

    printf("%s %d\n", *compareAsFiles, same);
    asObjectFreeList(&asThis);
    }
asObjectFreeList(&asStandard);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 3)
    usage();
asCompare(argv[1], argv+2);
optionFree();
return 0;
}
