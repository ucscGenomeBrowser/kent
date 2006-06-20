/* axtPretty - Convert axt to more human readable format.. 
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "axt.h"

static char const rcsid[] = "$Id: axtPretty.c,v 1.5 2006/06/20 16:44:16 angie Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtPretty - Convert axt to more human readable format.\n"
  "usage:\n"
  "   axtPretty in.axt out.pretty\n"
  "options:\n"
  "   -line=N Size of line, default 70\n"
  );
}


void axtOneOut(struct axt *axt, int lineSize, FILE *f)
/* Output axt in pretty format. */
{
char *q = axt->qSym;
char *t = axt->tSym;
int size = axt->symCount;
int oneSize, sizeLeft = size;
int i;

fprintf(f, ">%s:%d%c%d %s:%d-%d %d\n", 
	axt->qName, axt->qStart, axt->qStrand, axt->qEnd,
	axt->tName, axt->tStart, axt->tEnd, axt->score);
while (sizeLeft > 0)
    {
    oneSize = sizeLeft;
    if (oneSize > lineSize)
        oneSize = lineSize;
    mustWrite(f, q, oneSize);
    fputc('\n', f);

    for (i=0; i<oneSize; ++i)
        {
	if (toupper(q[i]) == toupper(t[i]) && isalpha(q[i]))
	    fputc('|', f);
	else
	    fputc(' ', f);
	}
    fputc('\n', f);

    if (oneSize > lineSize)
        oneSize = lineSize;
    mustWrite(f, t, oneSize);
    fputc('\n', f);
    fputc('\n', f);
    sizeLeft -= oneSize;
    q += oneSize;
    t += oneSize;
    }
}

void axtPretty(char *inName, char *outName)
/* axtPretty - Convert axt to more human readable format.. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
struct axt *axt;
int lineSize = optionInt("line", 70);

while ((axt = axtRead(lf)) != NULL)
    {
    axtOneOut(axt, lineSize, f);
    axtFree(&axt);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
axtPretty(argv[1], argv[2]);
return 0;
}
