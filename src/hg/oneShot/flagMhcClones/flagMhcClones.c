/* flagMhcClones - Look for clones Stephan wants in MHC.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "dnaseq.h"
#include "fa.h"
#include "portable.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "flagMhcClones - Look for clones Stephan wants in MHC.\n"
  "usage:\n"
  "   flagMhcClones mhcClones.txt gs.N\n");
}


void flagMhcClones(char *mhcFile, char *gsDir)
/* flagMhcClones - Look for clones Stephan wants in MHC.. */
{
struct lineFile *lf = lineFileOpen(mhcFile, TRUE);
char *line, *words[16];
int lineSize, wordCount, i;
char clonePath[512];
char *clone, *cloneVer;
static char *phases[3] = {"fin", "draft", "predraft",};
boolean found;

while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '#')
        continue;
    wordCount = chopLine(line, words);
    if (wordCount == 0)
        continue;
    lineFileExpectWords(lf, 7, wordCount);
    clone = words[0];
    cloneVer = words[1];
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
flagMhcClones(argv[1], argv[2]);
return 0;
}
