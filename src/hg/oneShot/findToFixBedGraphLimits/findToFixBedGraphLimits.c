/* findToFixBedGraphLimits - Scan through ra file of bedGraphs and calculate limits.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "ra.h"
#include "jksql.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "findToFixBedGraphLimits - Scan through ra file of bedGraphs and calculate limits.\n"
  "usage:\n"
  "   findToFixBedGraphLimits input.ra output.ra\n"
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

void findToFixBedGraphLimits(char *input, char *output)
/* findToFixBedGraphLimits - Scan through ra file of bedGraphs and calculate limits.. */
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
    sqlSafef(query, sizeof(query), "select %s from %s", field, track);
    struct sqlResult *sr = sqlGetResult(conn, query);
    char **row;
    row = sqlNextRow(sr);
    assert(row != NULL);
    double val = sqlDouble(row[0]);
    double minLimit = val, maxLimit = val;
    while ((row = sqlNextRow(sr)) != 0)
        {
	double val = sqlDouble(row[0]);
	if (val < minLimit) minLimit = val;
	if (val > maxLimit) maxLimit = val;
	}
    sqlFreeResult(&sr);
    verbose(1, "    %g %g\n",  minLimit, maxLimit);

    /* Output original table plus new minLimit/maxLimit. */
    for (el = list; el != NULL; el = el->next)
	fprintf(f, "%s %s\n", el->name, (char *)el->val);
    fprintf(f, "minLimit %g\n", minLimit);
    fprintf(f, "maxLimit %g\n", maxLimit);
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
findToFixBedGraphLimits(argv[1], argv[2]);
return 0;
}
