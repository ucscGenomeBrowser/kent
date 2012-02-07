/* axtPretty - Convert axt to more human readable format.. 
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "axt.h"


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


void axtPretty(char *inName, char *outName)
/* axtPretty - Convert axt to more human readable format.. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
struct axt *axt;
int lineSize = optionInt("line", 70);

while ((axt = axtRead(lf)) != NULL)
    {
    axtOutPretty(axt, lineSize, f);
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
