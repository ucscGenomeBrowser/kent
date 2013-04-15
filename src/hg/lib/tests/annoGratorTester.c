/* annoGratorTester -- exercise anno* lib modules (in kent/src as well as kent/src/hg) */

#include "annoGratorQuery.h"
#include "annoGratorGpVar.h"
#include "annoStreamBigBed.h"
#include "annoStreamDb.h"
#include "annoStreamTab.h"
#include "annoStreamVcf.h"
#include "annoStreamWig.h"
#include "annoGrateWigDb.h"
#include "annoFormatTab.h"
#include "annoFormatVep.h"
#include "dystring.h"
#include "hdb.h"
#include "memalloc.h"
#include "pgSnp.h"
#include "udc.h"
#include "vcf.h"
#if (defined USE_TABIX && defined KNETFILE_HOOKS)
#include "knetUdc.h"
#endif//def USE_TABIX && KNETFILE_HOOKS

// Names of tests:
static const char *pgSnpDbToTabOut = "pgSnpDbToTabOut";
static const char *pgSnpKgDbToTabOutShort = "pgSnpKgDbToTabOutShort";
static const char *pgSnpKgDbToTabOutLong = "pgSnpKgDbToTabOutLong";
static const char *pgSnpKgDbToGpFx = "pgSnpKgDbToGpFx";
static const char *snpConsDbToTabOutShort = "snpConsDbToTabOutShort";
static const char *snpConsDbToTabOutLong = "snpConsDbToTabOutLong";
static const char *vcfEx1 = "vcfEx1";
static const char *vcfEx2 = "vcfEx2";
static const char *bigBedToTabOut = "bigBedToTabOut";
static const char *snpBigWigToTabOut = "snpBigWigToTabOut";
static const char *vepOut = "vepOut";

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
    "    %s\n"
    "    %s\n"
    "    %s\n"
    "    %s\n"
    "    %s\n"
    "    %s\n"
    , pgSnpDbToTabOut, pgSnpKgDbToTabOutShort, pgSnpKgDbToTabOutLong,
    snpConsDbToTabOutShort, snpConsDbToTabOutLong,
    vcfEx1, vcfEx2, bigBedToTabOut, snpBigWigToTabOut, vepOut
    );
}

static struct optionSpec optionSpecs[] = {
    {NULL, 0}
};

//#*** duplicated from hgVarAnnoGrator... libify me!
struct annoAssembly *getAnnoAssembly(char *db)
/* Make annoAssembly for db. */
{
char *nibOrTwoBitDir = hDbDbNibPath(db);
char twoBitPath[HDB_MAX_PATH_STRING];
safef(twoBitPath, sizeof(twoBitPath), "%s/%s.2bit", nibOrTwoBitDir, db);
return annoAssemblyNew(db, twoBitPath);
}

struct streamerInfo
/* Enough info to create a streamer or grator that gets data from sql, file or URL. */
    {
    struct streamerInfo *next;
    struct annoAssembly *assembly;	// Reference assembly name and sequence.
    char *sqlDb;		// If non-NULL, then we are using this SQL database
    char *tableFileUrl;		// If db is non-NULL, table name; else file or URL
    enum annoRowType type;	// Data type (wig or words?)
    struct asObject *asObj;	// not used if type is arWig
    };

struct annoStreamer *streamerFromInfo(struct streamerInfo *info)
/* Figure out which constructor to call, call it and return the results. */
{
struct annoStreamer *streamer = NULL;
if (info->type == arWig)
    streamer = annoStreamWigDbNew(info->sqlDb, info->tableFileUrl, info->assembly, BIGNUM);
else if (info->sqlDb != NULL)
    streamer = annoStreamDbNew(info->sqlDb, info->tableFileUrl, info->assembly, info->asObj);
else if (info->type == arVcf) //#*** arVcf is bogus -- use autoSql comparison
    {
    boolean looksLikeTabix = endsWith(info->tableFileUrl, ".gz");
    streamer = annoStreamVcfNew(info->tableFileUrl, looksLikeTabix, info->assembly, BIGNUM);
    }
else if (endsWith(info->tableFileUrl, ".bb"))
    {
    streamer = annoStreamBigBedNew(info->tableFileUrl, info->assembly, BIGNUM);
    }
else
    {
    streamer = annoStreamTabNew(info->tableFileUrl, info->assembly, info->asObj);
    }
return streamer;
}

void sourcesFromInfoList(struct streamerInfo *infoList, bool doGpFx,
			 struct annoStreamer **retPrimary, struct annoGrator **retGrators)
/* Translate streamerInfo parameters into primary source and list of secondary sources. */
{
assert(infoList && retPrimary && retGrators);
struct streamerInfo *primaryInfo = infoList;
struct streamerInfo *gratorInfoList = infoList->next;
struct annoStreamer *primary = streamerFromInfo(primaryInfo);
struct annoGrator *gratorList = NULL;
struct streamerInfo *grInfo;
for (grInfo = gratorInfoList;  grInfo != NULL;  grInfo = grInfo->next)
    {
    struct annoGrator *grator = NULL;
    if (grInfo->type == arWig)
	{
	if (grInfo->sqlDb == NULL)
	    grator = annoGrateBigWigNew(grInfo->tableFileUrl, grInfo->assembly);
	else
	    grator = annoGrateWigDbNew(grInfo->sqlDb, grInfo->tableFileUrl, grInfo->assembly,
				       BIGNUM);
	}
    else
	{
	struct annoStreamer *src = streamerFromInfo(grInfo);
	if (doGpFx && gratorList == NULL) //#*** doGpFx should not be applied to all grators!
	    // #*** again, the real solution here is autoSql recognition
	    grator = annoGratorGpVarNew(src, FALSE);
	else
	    grator = annoGratorNew(src);
	}
    slAddHead(&gratorList, grator);
    }
slReverse(&gratorList);
*retPrimary = primary;
*retGrators = gratorList;
}

void dbToTabOut(struct streamerInfo *infoList, char *outFile,
		char *chrom, uint start, uint end, bool doGpFx)
/* Get data from one or more database tables and print all fields to tab-sep output. */
{
struct annoStreamer *primary = NULL;
struct annoGrator *gratorList = NULL;
sourcesFromInfoList(infoList, doGpFx, &primary, &gratorList);
struct annoFormatter *tabOut = annoFormatTabNew(outFile);
struct annoGratorQuery *query = annoGratorQueryNew(primary->assembly, primary, gratorList, tabOut);
annoGratorQuerySetRegion(query, chrom, start, end);
annoGratorQueryExecute(query);
annoGratorQueryFree(&query);
}

int main(int argc, char *argv[])
{
optionInit(&argc, argv, optionSpecs);
if (argc < 2 || argc > 3)
    usage();
pushCarefulMemHandler(LIMIT_2or6GB);
char *db = argv[1];
char *test = NULL;
boolean doAllTests = (argc == 2);
if (!doAllTests)
    {
    if (sameString(argv[2], pgSnpDbToTabOut) ||
	sameString(argv[2], pgSnpKgDbToTabOutShort) ||
	sameString(argv[2], pgSnpKgDbToTabOutLong) ||
	sameString(argv[2], pgSnpKgDbToGpFx) ||
	sameString(argv[2], snpConsDbToTabOutShort) ||
	sameString(argv[2], snpConsDbToTabOutLong) ||
	sameString(argv[2], vcfEx1) ||
	sameString(argv[2], vcfEx2) ||
	sameString(argv[2], bigBedToTabOut) ||
	sameString(argv[2], snpBigWigToTabOut) ||
	sameString(argv[2], vepOut))
	test = cloneString(argv[2]);
    else
	{
	warn("Unrecognized test name '%s'\n", argv[2]);
	usage();
	}
    }

if (udcCacheTimeout() < 300)
    udcSetCacheTimeout(300);
udcSetDefaultDir("./udcCache");

struct annoAssembly *assembly = getAnnoAssembly(db);

// First test: some rows of a pgSnp table
struct streamerInfo pgSnpInfo = { NULL, assembly, db, "pgNA12878", arWords, pgSnpAsObj() };
if (doAllTests || sameString(test, pgSnpDbToTabOut))
    dbToTabOut(&pgSnpInfo, "stdout", "chr1", 705881, 752721, FALSE);

// Second test: some rows of a pgSnp table integrated with knownGene
struct streamerInfo kgInfo = { NULL, assembly, db, "knownGene", arWords,
			       asParseFile("../knownGene.as") };
pgSnpInfo.next = &kgInfo;
if (doAllTests || sameString(test, pgSnpKgDbToTabOutShort))
    dbToTabOut(&pgSnpInfo, "stdout", "chr1", 705881, 752721, FALSE);

// Third test: all rows of a pgSnp table integrated with knownGene
if (doAllTests || sameString(test, pgSnpKgDbToTabOutLong))
    dbToTabOut(&pgSnpInfo, "stdout", NULL, 0, 0, FALSE);

// Fourth test: some rows of snp135 integrated with phyloP scores
if (doAllTests || sameString(test, snpConsDbToTabOutShort) ||
    sameString(test, snpConsDbToTabOutLong))
    {
    struct streamerInfo snp135Info = { NULL, assembly, db, "snp135", arWords,
				       asParseFile("../snp132Ext.as") };
    struct streamerInfo phyloPInfo = { NULL, assembly, db, "phyloP46wayPlacental", arWig, NULL };
    snp135Info.next = &phyloPInfo;
    if (sameString(test, snpConsDbToTabOutShort))
	dbToTabOut(&snp135Info, "stdout", "chr1", 737224, 738475, FALSE);
    else
	dbToTabOut(&snp135Info, "stdout", NULL, 0, 0, FALSE);
    }

// Fifth test: VCF with genotypes
if (doAllTests || sameString(test, vcfEx1))
    {
#if (defined USE_TABIX && defined KNETFILE_HOOKS)
    knetUdcInstall();
#endif//def USE_TABIX && KNETFILE_HOOKS
    struct streamerInfo vcfEx1 = { NULL, assembly, NULL,
			   "http://genome.ucsc.edu/goldenPath/help/examples/vcfExample.vcf.gz",
				   arVcf, vcfAsObj() };
    dbToTabOut(&vcfEx1, "stdout", NULL, 0, 0, FALSE);
    }

if (doAllTests || sameString(test, vcfEx2))
    {
    struct streamerInfo vcfEx2 = { NULL, assembly, NULL,
			   "http://genome.ucsc.edu/goldenPath/help/examples/vcfExampleTwo.vcf",
				   arVcf, vcfAsObj() };
    dbToTabOut(&vcfEx2, "stdout", NULL, 0, 0, FALSE);
    }

if (doAllTests || sameString(test, pgSnpKgDbToGpFx))
    {
    // intron +
    dbToTabOut(&pgSnpInfo, "stdout", "chr1", 161480984, 161481058, TRUE);

    // upstream +
    dbToTabOut(&pgSnpInfo, "stdout", "chr1", 161473787, 161475284, TRUE);

    // non-synonymous +
    dbToTabOut(&pgSnpInfo, "stdout", "chr1", 161476196, 161476223, TRUE);

    // non-synonymous - chr22:17,264,528-17,264,606
    dbToTabOut(&pgSnpInfo, "stdout", "chr22", 17264528, 17264606, TRUE);

    // synonymous - chr22:17,264,871-17,264,940
    dbToTabOut(&pgSnpInfo, "stdout", "chr22", 17264871, 17264940, TRUE);

    struct streamerInfo pg2SnpInfo = { NULL, assembly, db, "pgHG00733indel", arWords,
				       pgSnpAsObj() };
    pg2SnpInfo.next = &kgInfo;

    // 3base substitution CDS + chr1:21,806,596-21,806,642 
    dbToTabOut(&pg2SnpInfo, "stdout", "chr1", 21806596, 21806642, TRUE);

    // 6base deletion CDS - chr1:150,199,045-150,199,062
    dbToTabOut(&pg2SnpInfo, "stdout", "chr1", 150199045 , 150199062, TRUE);

    // 2base substitution CDS - chr1:152,185,806-152,185,825
    dbToTabOut(&pg2SnpInfo, "stdout", "chr1", 152185806 , 152185825, TRUE);

    // 6base cross codon deletion + chr1:87,045,875-87,045,934
    dbToTabOut(&pg2SnpInfo, "stdout", "chr1", 87045875,87045934, TRUE);

    // 2base deletion CDS + chr3:40,085,609-40,085,638
    dbToTabOut(&pg2SnpInfo, "stdout", "chr3",40085609,40085638, TRUE);

    /*
    FIXME
    // 3base insertion CDS - chr3:124,646,699-124,646,718
    dbToTabOut(&pg2SnpInfo, "stdout", "chr3",124646699,124646718, TRUE);
    */
    }

if (doAllTests || sameString(test, bigBedToTabOut))
    {
    struct streamerInfo bigBedInfo = { NULL, assembly, NULL,
			   "http://genome.ucsc.edu/goldenPath/help/examples/bigBedExample.bb",
				       arWords, NULL };
    dbToTabOut(&bigBedInfo, "stdout", "chr21", 34716800, 34733700, FALSE);
    }

if (doAllTests || sameString(test, snpBigWigToTabOut))
    {
    struct streamerInfo snp135Info = { NULL, assembly, db, "snp135", arWords,
				       asParseFile("../snp132Ext.as") };
    struct streamerInfo bigWigInfo = { NULL, assembly, NULL,
			   "http://genome.ucsc.edu/goldenPath/help/examples/bigWigExample.bw",
				       arWig, NULL };
    snp135Info.next = &bigWigInfo;
    dbToTabOut(&snp135Info, "stdout", "chr21", 34716800, 34733700, FALSE);
    }

if (doAllTests || sameString(test, vepOut))
    {
    struct streamerInfo vepSamplePgSnp = { NULL, assembly, NULL,
					   "input/annoGrator/vepSample.pgSnp.tab",
					   arWords, asParseFile("../pgSnp.as") };
    struct streamerInfo kgInfo = { NULL, assembly, db, "ensGene", arWords,
				   asParseFile("../genePredExt.as") };
    struct streamerInfo snpInfo = { NULL, assembly, db, "snp135", arWords,
				    asParseFile("../snp132Ext.as") };
    vepSamplePgSnp.next = &kgInfo;
    kgInfo.next = &snpInfo;
    // Instead of dbToTabOut, we need to make a VEP config data structure and
    // use it to create an annoFormatVep.
    struct streamerInfo *primaryInfo = &vepSamplePgSnp;
    struct annoStreamer *primary = NULL;
    struct annoGrator *gratorList = NULL;
    sourcesFromInfoList(primaryInfo, TRUE, &primary, &gratorList);
    struct annoFormatVepConfig config = { primary,
					  (struct annoStreamer *)gratorList,        // gpVar source
					  ((struct annoStreamer *)gratorList)->next,// dbSNP source
					  NULL				// no extra columns for now
					};

    struct annoFormatter *vepOut = annoFormatVepNew("stdout", &config);
    struct annoGratorQuery *query = annoGratorQueryNew(assembly, primary, gratorList, vepOut);
    annoGratorQuerySetRegion(query, "chr1", 876900, 886920);
    annoGratorQueryExecute(query);
    annoGratorQuerySetRegion(query, "chr5", 135530, 145535);
    annoGratorQueryExecute(query);
    annoGratorQueryFree(&query);
    }

return 0;
}
