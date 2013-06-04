/* findToFixBedGraphViewLimits - Calculate min/max/mean/std and use them to set viewLimits on bedGraphs described in input.ra. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "ra.h"
#include "hmmstats.h"
#include "jksql.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "findToFixBedGraphViewLimits - Calculate min/max/mean/std and use them to set viewLimits on bedGraphs described in input.ra\n"
  "usage:\n"
  "   findToFixBedGraphViewLimits input.ra output.ra\n"
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

void findToFixBedGraphViewLimits(char *input, char *output)
/* findToFixBedGraphViewLimits - Calculate min/max/mean/std and use them to set viewLimits on bedGraphs described in input.ra. */
{
struct lineFile *lf = lineFileOpen(input, TRUE);
FILE *f = mustOpen(output, "w");
struct slPair *el, *list;
while ((list = raNextRecordAsSlPairList(lf)) != NULL)
    {
    /* Find required fields for calcs. */
    char *db = mustFindVal(list, "db", lf);
    char *track = mustFindVal(list, "track", lf);
    char *type = cloneString(mustFindVal(list, "type", lf));

    /* Parse out type value, which should be "bedGraph 4" and put the 4 or whatever other number
     * in dataFieldIndex. */
    char *typeWords[3];
    int typeWordCount = chopLine(type, typeWords);
    if (typeWordCount != 2 || !sameString(typeWords[0], "bedGraph"))
           errAbort("Not well formed bedGraph type line %d of %s", lf->lineIx, lf->fileName);
    int dataFieldIndex = sqlUnsigned(typeWords[1]);
    freez(&type);

    /* Figure out field corresponding to dataFieldIndex. */
    struct sqlConnection *conn = sqlConnect(db);
    struct slName *fieldList = sqlFieldNames(conn, track);
    struct slName *pastBin = fieldList;
    if (sameString(pastBin->name, "bin"))
         pastBin = pastBin->next;
    struct slName *fieldName = slElementFromIx(pastBin, dataFieldIndex - 1);
    if (fieldName == NULL)
         errAbort("%s doesn't have enough fields", track);
    char *field = fieldName->name;
    assert(sqlFieldIndex(conn, track, field) >= 0);

    /* Print reassuring status message */
    verbose(1, "%s.%s has %d elements.  Data field is %s\n", db, track, sqlTableSize(conn, track), field);
         
    /* Get min/max dataValues in fields.  Do it ourselves rather than using SQL min/max because sometimes
     * the data field is a name column.... */
    char query[512];
    sqlSafef(query, sizeof(query), "select chromStart,chromEnd,%s from %s", field, track);
    struct sqlResult *sr = sqlGetResult(conn, query);
    char **row;
    row = sqlNextRow(sr);
    assert(row != NULL);
    int size = sqlUnsigned(row[1]) - sqlUnsigned(row[0]);
    double val = sqlDouble(row[2]);
    double minLimit = val, maxLimit = val;
    double sumData = val*size;
    double sumSquares = val*val*size;
    bits64 n = size;
    while ((row = sqlNextRow(sr)) != NULL)
        {
	val = sqlDouble(row[2]);
	size = sqlUnsigned(row[1]) - sqlUnsigned(row[0]);
	if (val < minLimit) minLimit = val;
	if (val > maxLimit) maxLimit = val;
	sumData += val*size;
	sumSquares += val*val*size;
	n += size;
	}
    sqlFreeResult(&sr);
    double mean = sumData/n;
    double std = calcStdFromSums(sumData, sumSquares, n);
    verbose(1, "    min=%g max=%g mean=%g std=%g\n",  minLimit, maxLimit, mean, std);

    double viewMax = mean + 6*std;
    if (viewMax > maxLimit) viewMax = maxLimit;
    double viewMin = mean - 6*std;
    if (viewMin < minLimit) viewMin = minLimit;

    /* Output original table plus new minLimit/maxLimit. */
    for (el = list; el != NULL; el = el->next)
	fprintf(f, "%s %s\n", el->name, (char *)el->val);
    fprintf(f, "new_minLimit %g\n", minLimit);
    fprintf(f, "new_maxLimit %g\n", maxLimit);
    fprintf(f, "new_mean %g\n", mean);
    fprintf(f, "new_std %g\n", std);
    fprintf(f, "new_viewLimits %g:%g\n", viewMin, viewMax);
    fprintf(f, "\n");

    sqlDisconnect(&conn);
    slFreeList(&fieldList);
    slPairFreeValsAndList(&list);
    }
lineFileClose(&lf);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
findToFixBedGraphViewLimits(argv[1], argv[2]);
return 0;
}
