/* roundNewViewLimitsInRa - Filter though .ra file rounding the new_viewLimits and making them viewLimits.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "ra.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "roundNewViewLimitsInRa - Filter though .ra file rounding the new_viewLimits and making them viewLimits.\n"
  "usage:\n"
  "   roundNewViewLimitsInRa in.ra out.ra\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

static char *mustFindVal(struct slPair *list, char *tag, struct lineFile *lf)
/* Look for tag in list and return value.  If none complain and abort. */
{
char *val = slPairFindVal(list, tag);
if (val == NULL)
    errAbort("missing required %s tag near line %d of %s", tag, lf->lineIx, lf->fileName);
return val;
}

void roundNewViewLimitsInRa(char *input, char *output)
/* roundNewViewLimitsInRa - Filter though .ra file rounding the new_viewLimits and making them 
 * viewLimits.. */
{
struct lineFile *lf = lineFileOpen(input, TRUE);
FILE *f = mustOpen(output, "w");
struct slPair *el, *list;
while ((list = raNextRecordAsSlPairList(lf)) != NULL)
    {
    double mean = atof(mustFindVal(list, "new_mean", lf));
    double std = atof(mustFindVal(list, "new_std", lf));
    double minLimit = atof(mustFindVal(list, "new_minLimit", lf));
    double maxLimit = atof(mustFindVal(list, "new_maxLimit", lf));

    double minV = mean - 6*std;
    if (minV < minLimit) minV = minLimit;
    double maxV = mean + 6*std;
    if (maxV > maxLimit) maxV = maxLimit;
    double minRound, maxRound;
    rangeRoundUp(minV, maxV, &minRound, &maxRound);

    for (el = list; el != NULL; el = el->next)
        {
	fprintf(f, "%s %s\n", el->name, (char*)el->val);
	}
    fprintf(f, "round_viewLimits %g:%g\n\n", minRound, maxRound);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
roundNewViewLimitsInRa(argv[1], argv[2]);
return 0;
}
