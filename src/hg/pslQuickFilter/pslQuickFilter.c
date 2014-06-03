/*
  File: pslQuickFilter.c
  Author: Terry Furey
  Date: 10/10/2003
  Description: Do a quick filter of a directory of psl files
*/

/* Copyright (C) 2006 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "portable.h"
#include "psl.h"
#include "options.h"

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"verbose", OPTION_BOOLEAN},
    {"minMatch", OPTION_INT},
    {"maxMismatch", OPTION_INT},
    {"maxTinsert", OPTION_INT},
    {NULL, 0}
};
boolean VERBOSE = FALSE;
int MINMATCH = 0;
int MAXMISMATCH = 100000;
int MAXTINSERT = 100000000;
 
void quickFilter(struct lineFile *pf, FILE *of)
/* Filter records based on simple input parameters */
{
  struct psl *psl;
   int lineSize;
   char *line;
   char *words[32];
   int  wordCount;
  
 /* Process each line in psl file */
 while (lineFileNext(pf, &line, &lineSize))
   {
     wordCount = chopTabs(line, words);
     if (wordCount != 21)
       errAbort("Bad line %d of %s\n", pf->lineIx, pf->fileName);
     if (words[15][0] == '-')
       continue;
     psl = pslLoad(words);
     if ((psl->match >= MINMATCH) &&
	 (psl->misMatch <= MAXMISMATCH) &&
	 (psl->tBaseInsert <= MAXTINSERT) &&
	 (psl->tBaseInsert >= 0) &&
	 (psl->qBaseInsert >= 0))
       pslTabOut(psl, of);
     pslFree(&psl);
   }
}

int main(int argc, char *argv[])
{
  struct lineFile *pf;
  FILE *of;
  char filename[512], outfile[512], *inDir, *outDir;
  struct slName *dirDir, *dirFile;

  optionInit(&argc, argv, optionSpecs);
  if (argc < 2)
    {
      fprintf(stderr, "USAGE: pslQuickFilter [-minMatch=N] [-maxMismatch=N] [-maxTinsert=N] -verbose <in psl dir> <out psl dir>\n");
      return 1;
    }
  VERBOSE = optionExists("verbose");
  MINMATCH = optionInt("minMatch",0);
  MAXMISMATCH = optionInt("maxMismatch",1000000);
  MAXTINSERT = optionInt("maxTinsert",1000000);

  inDir = argv[1];
  outDir = argv[2];
  dirDir = listDir(inDir, "*.psl");
  if (slCount(dirDir) == 0)
    errAbort("No psl files in %s\n", inDir);
  if (VERBOSE)
    printf("%s with %d files\n", inDir, slCount(dirDir));
  for (dirFile = dirDir; dirFile != NULL; dirFile = dirFile->next)
    {
      sprintf(filename, "%s/%s", inDir, dirFile->name);
      pf = pslFileOpen(filename);
      sprintf(outfile, "%s/%s", outDir, dirFile->name);
      of = mustOpen(outfile, "w");
      pslWriteHead(of);
      quickFilter(pf, of);
      lineFileClose(&pf);
      fclose(of);
    }
  slFreeList(&dirDir);

  return(0);
}
