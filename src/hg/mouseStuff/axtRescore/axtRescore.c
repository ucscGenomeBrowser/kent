/* axtRescore - Recalculate scores in axt. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "axt.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtRescore - Recalculate scores in axt.\n"
  "usage:\n"
  "   axtRescore in.axt out.axt\n"
  "options:\n"
  "   -scoreScheme=fileName Read the scoring matrix from a blastz-format file.\n"
  );
}

struct axtScoreScheme *scoreScheme = NULL;  /* -scoreScheme flag. */

void axtRescore(char *in, char *out)
/* axtRescore - Recalculate scores in axt. */
{
struct lineFile *lf = lineFileOpen(in, TRUE);
FILE *f = mustOpen(out, "w");
struct axt *axt;

lineFileSetMetaDataOutput(lf, f);
axtScoreSchemeDnaWrite(scoreScheme, f, "axtRescore");
for (;;)
    {
    axt = axtRead(lf);
    if (axt == NULL)
        break;
    axt->score = axtScore(axt, scoreScheme);
    axtWrite(axt, f);
    axtFree(&axt);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *scoreSchemeFile = NULL;
optionHash(&argc, argv);
if (argc != 3)
    usage();
scoreSchemeFile = optionVal("scoreScheme", scoreSchemeFile);
if (scoreSchemeFile != NULL)
    scoreScheme = axtScoreSchemeRead(scoreSchemeFile);
else
    scoreScheme = axtScoreSchemeDefault();
axtRescore(argv[1], argv[2]);
return 0;
}
