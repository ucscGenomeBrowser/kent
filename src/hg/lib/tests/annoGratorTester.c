/* annoGratorTester -- exercise anno* lib modules (in kent/src as well as kent/src/hg) */

#include "annoGratorQuery.h"
#include "annoStreamDb.h"
#include "annoStreamWig.h"
#include "annoGrateWig.h"
#include "annoFormatTab.h"
#include "dystring.h"
#include "pgSnp.h"

// Names of tests:
static const char *pgSnpDbToTabOut = "pgSnpDbToTabOut";
static const char *pgSnpKgDbToTabOutShort = "pgSnpKgDbToTabOutShort";
static const char *pgSnpKgDbToTabOutLong = "pgSnpKgDbToTabOutLong";
static const char *snpConsDbToTabOutShort = "snpConsDbToTabOutShort";

void usage()
/* explain usage and exit */
{
errAbort(
    "annoGratorTester - test program for anno* lib modules\n\n"
    "usage:\n"
    "    annoGratorTester db [testName]\n"
//    "options:\n"
    "testName can be one of the following:\n"
    "    %s\n"
    "    %s\n"
    "    %s\n"
    "    %s\n"
    , pgSnpDbToTabOut, pgSnpKgDbToTabOutShort, pgSnpKgDbToTabOutLong, snpConsDbToTabOutShort
    );
}

static struct optionSpec optionSpecs[] = {
    {NULL, 0}
};

struct dbSource
/* Enough info to create a streamer or grator that gets data from sql. */
    {
    struct dbSource *next;
    char *db;
    char *table;
    enum annoRowType type;
    struct asObject *asObj; // not used if type is arWig
    };

void dbToTabOut(struct dbSource *srcList, struct twoBitFile *tbf, char *outFile,
		char *chrom, uint start, uint end)
/* Get data from one or more database tables and print all fields to tab-sep output. */
{
struct annoStreamer *primary;
if (srcList->type == arWig)
    primary = annoStreamWigDbNew(srcList->db, srcList->table, BIGNUM);
else
    primary = annoStreamDbNew(srcList->db, srcList->table, srcList->asObj);
struct annoGrator *gratorList = NULL;
struct dbSource *src;
for (src = srcList->next;  src != NULL;  src = src->next)
    {
    if (src->type == arWig)
	slAddHead(&gratorList, annoGrateWigDbNew(src->db, src->table, BIGNUM));
    else
	{
	struct annoStreamer *str = annoStreamDbNew(src->db, src->table, src->asObj);
	slAddHead(&gratorList, annoGratorNew(str));
	}
    }
slReverse(&gratorList);
struct annoFormatter *tabOut = annoFormatTabNew(outFile);
struct annoGratorQuery *query = annoGratorQueryNew(srcList->db, NULL, tbf,
						   primary, gratorList, tabOut);
annoGratorQuerySetRegion(query, chrom, start, end);
annoGratorQueryExecute(query);
annoGratorQueryFree(&query);
}

int main(int argc, char *argv[])
{
optionInit(&argc, argv, optionSpecs);
if (argc < 2 || argc > 3)
    usage();
char *db = argv[1];
char *test = NULL;
boolean doAllTests = (argc == 2);
if (!doAllTests)
    {
    if (sameString(argv[2], pgSnpDbToTabOut) ||
	sameString(argv[2], pgSnpKgDbToTabOutShort) ||
	sameString(argv[2], pgSnpKgDbToTabOutLong) ||
	sameString(argv[2], snpConsDbToTabOutShort))
	test = cloneString(argv[2]);
    else
	{
	warn("Unrecognized test name '%s'\n", argv[2]);
	usage();
	}
    }

struct dyString *dbDotTwoBit = dyStringCreate("/hive/data/genomes/%s/%s.2bit", db, db);
struct twoBitFile *tbf = twoBitOpen(dbDotTwoBit->string);

// First test: some rows of a pgSnp table
struct dbSource src1 = { NULL, db, "pgNA12878", arWords, pgSnpAsObj() };
if (doAllTests || sameString(test, pgSnpDbToTabOut))
    dbToTabOut(&src1, tbf, "stdout", "chr1", 705881, 752721);

// Second test: some rows of a pgSnp table integrated with knownGene
struct dbSource src2 = { NULL, db, "knownGene", arWords, asParseFile("../knownGene.as") };
src1.next = &src2;
if (doAllTests || sameString(test, pgSnpKgDbToTabOutShort))
    dbToTabOut(&src1, tbf, "stdout", "chr1", 705881, 752721);

// Third test: all rows of a pgSnp table integrated with knownGene
if (doAllTests || sameString(test, pgSnpKgDbToTabOutLong))
    dbToTabOut(&src1, tbf, "stdout", NULL, 0, 0);

// Fourth test: some rows of snp135 integrated with phyloP scores
struct dbSource src3 = { NULL, db, "snp135", arWords, asParseFile("../snp132Ext.as") };
struct dbSource src4 = { NULL, db, "phyloP46wayPlacental", arWig, NULL };
src3.next = &src4;
if (doAllTests || sameString(test, snpConsDbToTabOutShort))
    dbToTabOut(&src3, tbf, "stdout", "chr1", 737224, 738475);

return 0;
}
