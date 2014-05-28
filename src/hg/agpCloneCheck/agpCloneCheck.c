/* agpCloneCheck - Check that have all clones in an agp file (and the right version too). */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "fa.h"
#include "portable.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "agpCloneCheck - Check that have all clones in an agp file (and the right version too)\n"
  "usage:\n"
  "   agpCloneCheck file.agp gsDir\n"
  );
}

void agpCloneCheck(char *agpFile, char *gsDir)
/* agpCloneCheck - Check that have all clones in an agp file (and the right version too). */
{
struct lineFile *lf = lineFileOpen(agpFile, TRUE);
char *line, *words[16];
int lineSize, wordCount, i;
char clonePath[512];
char clone[128], *cloneVer;
static char *phases[3] = {"fin", "draft", "predraft",};
boolean found;

while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '#')
        continue;
    wordCount = chopLine(line, words);
    if (wordCount == 0)
        continue;
    if (wordCount < 5)
        errAbort("Bad line %d of %s", lf->lineIx, lf->fileName);
    if (words[4][0] == 'N' || words[4][0] == 'U')
        continue;
    cloneVer = words[5];
    strcpy(clone, cloneVer);
    chopSuffix(clone);
    found = FALSE;
    for (i = 0; i < 3; ++i)
        {
	char *phase = phases[i];
	sprintf(clonePath, "%s/%s/fa/%s.fa", gsDir, phase, clone);
	if (fileExists(clonePath))
	    {
	    struct dnaSeq *seq = faReadDna(clonePath);
	    char *e = strchr(seq->name, '_');

	    if (e != NULL) *e = 0;
	    if (!sameString(seq->name, cloneVer))
		printf("%s\t(wrong version %s)\n", cloneVer, seq->name);
	    else if (i != 0)
	        printf("%s\t(not finished)\n", cloneVer);
	    found = TRUE;
	    }
	}
    if (!found)
        printf("%s\t(not found)\n", cloneVer);
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 3)
    usage();
agpCloneCheck(argv[1], argv[2]);
return 0;
}
