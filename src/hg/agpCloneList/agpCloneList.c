/* agpCloneList - Make simple list of all clones in agp file. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"


boolean ver = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "agpCloneList - Make simple list of all clones in agp file\n"
  "to stdout\n"
  "usage:\n"
  "   agpCloneList file(s).agp\n"
  "options:\n"
  "   -ver print clone version as well as name.\n"
  );
}

void agpCloneList(int agpCount, char *agpNames[])
/* agpCloneList - Make simple list of all clones in agp file. */
{
struct hash *hash = newHash(14);
int i;
char *fileName;
struct lineFile *lf = NULL;
char cloneName[256];
char *words[32];
int wordCount;

for (i=0; i<agpCount; ++i)
    {
    fileName = agpNames[i];
    lf = lineFileOpen(fileName, TRUE);
    while ((wordCount = lineFileChop(lf, words)) > 0)
        {
	if (wordCount < 6)
	    errAbort("Expecting at least 6 words line %d of %s", lf->lineIx, lf->fileName);
	if (words[4][0] != 'N' && words[4][0] != 'U')
	    {
	    strcpy(cloneName, words[5]);
	    chopSuffix(cloneName);
	    if (!hashLookup(hash, cloneName))
	        {
		if (ver)
		    printf("%s\n", words[5]);
		else
		    printf("%s\n", cloneName);
		hashAdd(hash, cloneName, NULL);
		}
	    }
	}
    lineFileClose(&lf);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
ver = cgiBoolean("ver");
if (argc < 2)
    usage();
agpCloneList(argc-1, argv+1);
return 0;
}
