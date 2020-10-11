#include "common.h"
#include "wiggle.h"
#include "cart.h"
#include "hgc.h"
#include "hCommon.h"
#include "hgColors.h"
#include "bigBed.h"
#include "hui.h"
#include "subText.h"   
#include "trackHub.h"   
#include "clinvarSub.h"   
#include "clinvarSubLolly.h"   

static void printVariant(struct trackDb *tdb, struct bbiFile *bbi, struct bigBedInterval *bbList, char *variant)
{
struct bigBedInterval *bb;
for (bb = bbList; bb != NULL; bb = bb->next)
    {
    char *fields[bbi->fieldCount];
    int restCount = chopTabs(cloneString(bb->rest), fields);
    if (differentString(fields[36], variant))
        continue;
    int restBedFields = 12 - 3;
    char **extraFields = (fields + restBedFields);
    int extraFieldCount = restCount - restBedFields;
    extraFieldsPrint(tdb,NULL,extraFields, extraFieldCount);
    }
}

static void printVariants(struct trackDb *tdb, char *variants, char *chrom, int start, int end)
{
int count = chopByChar(variants, ',', NULL, 0);
char *words[count];
chopByChar(variants, ',', words, count);
char *xrefTrack = trackDbSetting(tdb, "xrefTrack");
struct trackDb *mainTdb = hashFindVal(trackHash, xrefTrack);
char *mainBigBedFile = trackDbSetting(mainTdb, "bigDataUrl");
struct bbiFile *bbi = bigBedFileOpen(mainBigBedFile);
struct lm *lm = lmInit(0);
struct bigBedInterval *bbList = bigBedIntervalQuery(bbi, chrom, start, end, 0, lm);

printf("There are %d variants at this position.\n", count);
int ii;
for(ii=0; ii < count; ii++)
    {
    printVariant(mainTdb, bbi, bbList, words[ii]);
    }
}


static char *subFields[] =
{
"VariationID",
"ClinicalSignificance",
"DateLastEvaluated",
"Description",
"SubmittedPhenotypeInfo",
"ReportedPhenotypeInfo",
"ReviewStatus",
"CollectionMethod",
"OriginCounts",
"Submitter",
"SCV",
"SubmittedGeneSymbol",
};

static void printSub(struct sqlConnection *conn,  char *sub)
{
char query[4096];
sqlSafef(query, sizeof(query), "select * from clinvarSub where scv = '%s'", sub);
struct sqlResult *sr;
sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    printf("<br><table class='bedExtraTbl'>");
    int ii;
    for(ii=0; ii < ArraySize(subFields); ii++)
        printf("<tr><td>%s</td><td>%s</td>", subFields[ii], row[ii]);
    printf("</table>\n");
    }
}
static void printSubs(struct sqlConnection *conn, char *subs, char *chrom, int start, int end)
{
int count = chopByChar(subs, ',', NULL, 0);
char *words[count];
chopByChar(subs, ',', words, count);

printf("There are %d submissions at this position.\n", count);
int ii;
for(ii=0; ii < count; ii++)
    {
    printSub(conn, words[ii]);
    }
}

void doClinvarSubLolly(struct trackDb *tdb, char *item)
/* Put up page for clinvarSubLolly track. */
{
genericHeader(tdb, NULL);
struct sqlConnection *conn = NULL;

if (!trackHubDatabase(database))
    conn = hAllocConnTrack(database, tdb);

int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
char *chrom = cartString(cart, "c");
char *fileName = bbiNameFromSettingOrTable(tdb, conn, tdb->table);
struct bbiFile *bbi = bigBedFileOpen(fileName);
struct lm *lm = lmInit(0);
struct bigBedInterval *bbList = bigBedIntervalQuery(bbi, chrom, start, end, 0, lm);
int  bedSize = bbi->definedFieldCount;

struct bigBedInterval *bb;
char *fields[bbi->fieldCount];
for (bb = bbList; bb != NULL; bb = bb->next)
    {
    if (!(bb->start == start && bb->end == end))
	continue;
    char *name = cloneFirstWordByDelimiterNoSkip(bb->rest, '\t');
    boolean match = (isEmpty(name) && isEmpty(item)) || sameOk(name, item);
    if (!match)
        continue;
    char startBuf[16], endBuf[16];
    bigBedIntervalToRow(bb, chrom, startBuf, endBuf, fields, bbi->fieldCount);
    struct bed *bed = bedLoadN(fields, bedSize);
    bedPrintPos(bed, bedSize, tdb);
    printSubs(conn, fields[12], chrom, start, end);
    printVariants(tdb, fields[11], chrom, start, end);
    break;
    }
}
