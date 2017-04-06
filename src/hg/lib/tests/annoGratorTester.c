/* annoGratorTester -- exercise anno* lib modules (in kent/src as well as kent/src/hg) */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

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
#include "bigBed.h"
#include "dystring.h"
#include "genePred.h"
#include "hdb.h"
#include "knetUdc.h"
#include "memalloc.h"
#include "pgSnp.h"
#include "udc.h"
#include "vcf.h"

//#*** duplicated from hgVarAnnoGrator... libify me!
struct annoAssembly *getAnnoAssembly(char *db)
/* Make annoAssembly for db. */
{
static struct annoAssembly *aa = NULL;
if (aa == NULL)
    {
    char *nibOrTwoBitDir = hDbDbNibPath(db);
    if (nibOrTwoBitDir == NULL)
        errAbort("Can't find .2bit for db '%s'", db);
    char twoBitPath[HDB_MAX_PATH_STRING];
    safef(twoBitPath, sizeof(twoBitPath), "%s/%s.2bit", nibOrTwoBitDir, db);
    char *path = hReplaceGbdb(twoBitPath);
    aa = annoAssemblyNew(db, path);
    freeMem(path);
    }
return aa;
}

struct streamerInfo
/* Enough info to create a streamer or grator that gets data from sql, file or URL. */
    {
    struct streamerInfo *next;
    struct annoAssembly *assembly;	// Reference assembly name and sequence.
    char *sqlDb;		// If non-NULL, then we are using this SQL database
    char *tableFileUrl;		// If db is non-NULL, table name; else file or URL
    enum annoRowType type;	// Data type (wig or words?)
    struct asObject *asObj;	// not used if we're using a sqlDb table
    };

struct annoStreamer *streamerFromInfo(struct streamerInfo *info)
/* Figure out which constructor to call, call it and return the results. */
{
struct annoStreamer *streamer = NULL;
if (info->type == arWigVec)
    streamer = annoStreamWigDbNew(info->sqlDb, info->tableFileUrl, info->assembly, BIGNUM);
else if (info->sqlDb != NULL)
    streamer = annoStreamDbNew(info->sqlDb, info->tableFileUrl, info->assembly, BIGNUM, NULL);
else if (info->asObj && asObjectsMatch(info->asObj, vcfAsObj()))
    {
    //#*** this is kludgey, should test for .tbi file:
    boolean looksLikeTabix = endsWith(info->tableFileUrl, ".gz");
    streamer = annoStreamVcfNew(info->tableFileUrl, NULL, looksLikeTabix, info->assembly, BIGNUM);
    }
else if (endsWith(info->tableFileUrl, ".bb"))
    {
    streamer = annoStreamBigBedNew(info->tableFileUrl, info->assembly, BIGNUM);
    }
else
    {
    streamer = annoStreamTabNew(info->tableFileUrl, info->assembly, info->asObj, BIGNUM);
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
    if (grInfo->type == arWigVec || grInfo->type == arWigSingle)
	{
	if (grInfo->sqlDb == NULL)
	    grator = annoGrateBigWigNew(grInfo->tableFileUrl, grInfo->assembly, agwmAverage);
	else
	    grator = annoGrateWigDbNew(grInfo->sqlDb, grInfo->tableFileUrl, grInfo->assembly,
				       agwmAverage, BIGNUM);
	}
    else
	{
	struct annoStreamer *src = streamerFromInfo(grInfo);
	if (doGpFx && grInfo->asObj && asColumnNamesMatchFirstN(grInfo->asObj, genePredAsObj(), 10))
	    grator = annoGratorGpVarNew(src);
	else
	    grator = annoGratorNew(src);
	}
    slAddHead(&gratorList, grator);
    }
slReverse(&gratorList);
*retPrimary = primary;
*retGrators = gratorList;
}

struct asObject *bigBedAsFromFileName(char *fileName)
/* Look up bigBed filename in table and get its internally stored autoSql definition. */
{
struct bbiFile *bbi = bigBedFileOpen(fileName);
struct asObject *asObj = bigBedAs(bbi);
bigBedFileClose(&bbi);
return asObj;
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

void pgSnpDbToTabOut(struct annoAssembly *assembly)
// First test: some rows of a pgSnp table
{
char *sqlDb = assembly->name;
struct streamerInfo pgSnpInfo = { NULL, assembly, sqlDb, "pgNA12878", arWords, pgSnpAsObj() };
pgSnpInfo.next = NULL;
dbToTabOut(&pgSnpInfo, "stdout", "chr1", 705881, 752721, FALSE);
}

void pgSnpKgDbToTabOutShort(struct annoAssembly *assembly)
// Second test: some rows of a pgSnp table integrated with knownGene
{
char *sqlDb = assembly->name;
struct streamerInfo pgSnpInfo = { NULL, assembly, sqlDb, "pgNA12878", arWords, pgSnpAsObj() };
struct streamerInfo kgInfo = { NULL, assembly, sqlDb, "knownGene", arWords,
                               asParseFile("../knownGene.as") };
pgSnpInfo.next = &kgInfo;
dbToTabOut(&pgSnpInfo, "stdout", "chr1", 705881, 752721, FALSE);
}

void pgSnpKgDbToTabOutLong(struct annoAssembly *assembly)
// Third test: all rows of a pgSnp table integrated with knownGene
{
char *sqlDb = assembly->name;
struct streamerInfo pgSnpInfo = { NULL, assembly, sqlDb, "pgNA12878", arWords, pgSnpAsObj() };
dbToTabOut(&pgSnpInfo, "stdout", NULL, 0, 0, FALSE);
}

void snpConsDbToTabOutShort(struct annoAssembly *assembly)
// Fourth test: some rows of snp135 integrated with phyloP scores
{
char *sqlDb = assembly->name;
struct streamerInfo snp135Info = { NULL, assembly, sqlDb, "snp135", arWords,
                                   asParseFile("../snp132Ext.as") };
struct streamerInfo phyloPInfo = { NULL, assembly, sqlDb, "phyloP46wayPlacental", arWigSingle,
                                   NULL };
snp135Info.next = &phyloPInfo;
dbToTabOut(&snp135Info, "stdout", "chr1", 737224, 738475, FALSE);
}

void snpConsDbToTabOutLong(struct annoAssembly *assembly)
// Long-running!: All rows of snp135 integrated with phyloP scores
{
char *sqlDb = assembly->name;
struct streamerInfo snp135Info = { NULL, assembly, sqlDb, "snp135", arWords,
                                   asParseFile("../snp132Ext.as") };
struct streamerInfo phyloPInfo = { NULL, assembly, sqlDb, "phyloP46wayPlacental", arWigSingle,
                                   NULL };
snp135Info.next = &phyloPInfo;
dbToTabOut(&snp135Info, "stdout", NULL, 0, 0, FALSE);
}

void vcfEx1(struct annoAssembly *assembly)
// Fifth test: VCF with genotypes
{
struct streamerInfo vcfEx1 = { NULL, assembly, NULL,
                               "http://genome.ucsc.edu/goldenPath/help/examples/vcfExample.vcf.gz",
                               arWords, vcfAsObj() };
dbToTabOut(&vcfEx1, "stdout", NULL, 0, 0, FALSE);
}

void vcfEx2(struct annoAssembly *assembly)
// VCF with no genotypes
{
struct streamerInfo vcfEx2 = { NULL, assembly, NULL,
                               "http://genome.ucsc.edu/goldenPath/help/examples/vcfExampleTwo.vcf",
                               arWords, vcfAsObj() };
dbToTabOut(&vcfEx2, "stdout", NULL, 0, 0, FALSE);
}

void pgSnpKgDbToGpFx(struct annoAssembly *assembly)
// pgSnp + knownGene + gpFx = annotated variants
{
char *sqlDb = assembly->name;
struct streamerInfo pg2SnpInfo = { NULL, assembly, NULL,
                                   "input/annoGrator/pgForTestingGpFx.pgSnp.tab",
                                   arWords, pgSnpAsObj() };
struct streamerInfo kgInfo = { NULL, assembly, sqlDb, "knownGene", arWords,
                               asParseFile("../knownGene.as") };
pg2SnpInfo.next = &kgInfo;
dbToTabOut(&pg2SnpInfo, "stdout", NULL, 0, 0, TRUE);

/*
  FIXME
  // 3base insertion CDS - chr3:124,646,699-124,646,718
  dbToTabOut(&pg2SnpInfo, "stdout", "chr3",124646699,124646718, TRUE);
*/
}

void bigBedToTabOut(struct annoAssembly *assembly)
// like bigBedToBed
{
struct streamerInfo bigBedInfo = { NULL, assembly, NULL,
                               "http://genome.ucsc.edu/goldenPath/help/examples/bigBedExample.bb",
                                   arWords, NULL };
dbToTabOut(&bigBedInfo, "stdout", "chr21", 34716800, 34733700, FALSE);
}

void snpBigWigToTabOut(struct annoAssembly *assembly)
// text + scores
{
char *sqlDb = assembly->name;
struct streamerInfo snp135Info = { NULL, assembly, sqlDb, "snp135", arWords,
                                   asParseFile("../snp132Ext.as") };
struct streamerInfo bigWigInfo = { NULL, assembly, NULL,
                               "http://genome.ucsc.edu/goldenPath/help/examples/bigWigExample.bw",
                                   arWigSingle, NULL };
snp135Info.next = &bigWigInfo;
dbToTabOut(&snp135Info, "stdout", "chr21", 34716800, 34733700, FALSE);
}

void vepOut(struct annoAssembly *assembly)
// variants + genes + gpFx + snps + annoFormatVep = annotated variants in VEP format
{
char *sqlDb = assembly->name;
struct streamerInfo vepSamplePgSnp = { NULL, assembly, NULL,
                                       "input/annoGrator/vepSample.pgSnp.tab",
                                       arWords, asParseFile("../pgSnp.as") };
struct streamerInfo ensGInfo = { NULL, assembly, sqlDb, "ensGene", arWords,
                               asParseFile("../genePredExt.as") };
struct streamerInfo snpInfo = { NULL, assembly, sqlDb, "snp135", arWords,
                                asParseFile("../snp132Ext.as") };
vepSamplePgSnp.next = &ensGInfo;
ensGInfo.next = &snpInfo;
// Instead of dbToTabOut, we need to make a VEP config data structure and
// use it to create an annoFormatVep.
struct streamerInfo *primaryInfo = &vepSamplePgSnp;
struct annoStreamer *primary = NULL;
struct annoGrator *gratorList = NULL;
sourcesFromInfoList(primaryInfo, TRUE, &primary, &gratorList);
struct annoStreamer *gpVarSource = (struct annoStreamer *)gratorList;
struct annoStreamer *snpSource = gpVarSource->next;
struct annoFormatter *vepOut = annoFormatVepNew("stdout", FALSE, primary, "vepSamplePgSnp",
                                                gpVarSource, "UCSC Genes ...",
                                                snpSource, "just dbSNP 135", assembly);
struct annoGratorQuery *query = annoGratorQueryNew(assembly, primary, gratorList, vepOut);
annoGratorQuerySetRegion(query, "chr1", 876900, 886920);
annoGratorQueryExecute(query);
annoGratorQuerySetRegion(query, "chr5", 135530, 145535);
annoGratorQueryExecute(query);
annoGratorQueryFree(&query);
}

void vepOutIndelTrim(struct annoAssembly *assembly)
// variants with VCF's awful indel coordinates + ... = VEP
{
char *sqlDb = assembly->name;
struct streamerInfo indelTrimVcf = { NULL, assembly, NULL,
                                     "input/annoGrator/indelTrim.vcf",
                                     arWords, vcfAsObj() };
struct streamerInfo gencodeInfo = { NULL, assembly, sqlDb, "wgEncodeGencodeBasicV19", arWords,
                                    asParseFile("../genePredExt.as") };
indelTrimVcf.next = &gencodeInfo;
// Instead of dbToTabOut, we need to make a VEP config data structure and
// use it to create an annoFormatVep.
struct streamerInfo *primaryInfo = &indelTrimVcf;
struct annoStreamer *primary = NULL;
struct annoGrator *gratorList = NULL;
sourcesFromInfoList(primaryInfo, TRUE, &primary, &gratorList);
struct annoStreamer *gpVarSource = (struct annoStreamer *)gratorList;
struct annoFormatter *vepOut = annoFormatVepNew("stdout", FALSE, primary, "indelTrimVcf",
                                                gpVarSource, "EnsemblGenes ...",
                                                NULL, NULL, assembly);
struct annoGratorQuery *query = annoGratorQueryNew(assembly, primary, gratorList, vepOut);
annoGratorQuerySetRegion(query, "chr11", 0, 0);
annoGratorQueryExecute(query);
annoGratorQueryFree(&query);
}

void gpFx(struct annoAssembly *assembly)
// Add in dbNsfp data for missense variants
{
char *sqlDb = assembly->name;
struct streamerInfo variants = { NULL, assembly, NULL,
                                 "input/annoGrator/moreVariants.pgSnp.tab",
                                 arWords, asParseFile("../pgSnp.as") };
struct streamerInfo kgInfo = { NULL, assembly, sqlDb, "knownGene", arWords,
                               asParseFile("../knownGene.as") };
struct streamerInfo snpInfo = { NULL, assembly, sqlDb, "snp137", arWords,
                                asParseFile("../snp132Ext.as") };
struct asObject *dbNsfpSeqChangeAs =
    bigBedAsFromFileName("/gbdb/hg19/dbNsfp/dbNsfpSeqChange.bb");
struct streamerInfo dbNsfpSeqChange =
    { NULL, assembly, NULL, "/gbdb/hg19/dbNsfp/dbNsfpSeqChange.bb",
      arWords, dbNsfpSeqChangeAs };
struct asObject *dbNsfpSiftAs = bigBedAsFromFileName("/gbdb/hg19/dbNsfp/dbNsfpSift.bb");
struct streamerInfo dbNsfpSift = { NULL, assembly, NULL, "/gbdb/hg19/dbNsfp/dbNsfpSift.bb",
                                   arWords, dbNsfpSiftAs };
variants.next = &kgInfo;
kgInfo.next = &snpInfo;
snpInfo.next = &dbNsfpSeqChange;
dbNsfpSeqChange.next = &dbNsfpSift;
// Instead of dbToTabOut, we need to make a VEP config data structure and
// use it to create an annoFormatVep.
struct streamerInfo *primaryInfo = &variants;
struct annoStreamer *primary = NULL;
struct annoGrator *gratorList = NULL;
sourcesFromInfoList(primaryInfo, TRUE, &primary, &gratorList);
struct annoStreamer *gpVarSource = (struct annoStreamer *)gratorList;
struct annoStreamer *snpSource = gpVarSource->next;
struct annoStreamer *dbNsfpSource = snpSource->next->next;
struct annoFormatter *vepOut = annoFormatVepNew("stdout", FALSE, primary, "some more variants",
                                                gpVarSource, "UCSC Genes of course",
                                                snpSource, "now snp137.", assembly);
annoFormatVepAddExtraItem(vepOut, dbNsfpSource, "SIFT", "SIFT score from dbNSFP", "", FALSE);
struct annoGratorQuery *query = annoGratorQueryNew(assembly, primary, gratorList, vepOut);
annoGratorQuerySetRegion(query, "chr19", 45405960, 45419476);
annoGratorQueryExecute(query);
annoGratorQueryFree(&query);
}

void doInsertionsRegions(struct streamerInfo *infoList)
/* Perform a series of region queries on infoList for the 'insertions' test. */
{
// Entire range of features in both primary and secondary:
puts("# region: chr1   0  500");
dbToTabOut(infoList, "stdout", "chr1", 0, 500, FALSE);
// Region to the left of insLeft:
puts("# region: chr1 100  200");
dbToTabOut(infoList, "stdout", "chr1", 100, 200, FALSE);
// Region to the right of insLeft and left of insRight:
puts("# region: chr1 200  300");
dbToTabOut(infoList, "stdout", "chr1", 200, 300, FALSE);
// Region to the right of insRight and left of pi (and insPi):
puts("# region: chr1 300  400");
dbToTabOut(infoList, "stdout", "chr1", 300, 400, FALSE);
// Region to the right of pi (and insPi):
puts("# region: chr1 400  500");
dbToTabOut(infoList, "stdout", "chr1", 400, 500, FALSE);
}

void insertions(struct annoAssembly *assembly)
// Test corner cases of intersection of zero-length insertions with regular items (length > 0)
// and with different search regions to make sure that insertions at edges are included.
{
struct asObject *bed4AS = asParseFile("../bed.as");
struct streamerInfo primary = { NULL, assembly, NULL,
                                "input/annoGrator/insertionsPrimary.bed",
                                arWords, bed4AS };
struct streamerInfo secondary = { NULL, assembly, NULL,
                                  "input/annoGrator/insertionsSecondary.bed",
                                  arWords, bed4AS };
primary.next = &secondary;

// Plain BED files
puts("# BED files");
doInsertionsRegions(&primary);

// BigBed versions of same files
puts("# BigBed files");
primary.tableFileUrl = "input/annoGrator/insertionsPrimary.bb";
secondary.tableFileUrl = "input/annoGrator/insertionsSecondary.bb";
doInsertionsRegions(&primary);

// Mysql tables from BED files
puts("# BED tables");
primary.sqlDb = secondary.sqlDb = "test";
primary.tableFileUrl = "insertionsPrimary";
secondary.tableFileUrl = "insertionsSecondary";
doInsertionsRegions(&primary);

// Uncompressed VCF
puts("# VCF files (uncompressed)");
primary.sqlDb = secondary.sqlDb = NULL;
primary.tableFileUrl = "input/annoGrator/insertionsPrimary.vcf";
secondary.tableFileUrl = "input/annoGrator/insertionsSecondary.vcf";
primary.asObj = secondary.asObj = vcfAsObj();
doInsertionsRegions(&primary);

// VCF+tabix
puts("# VCF files (tabix)");
primary.tableFileUrl = "input/annoGrator/insertionsPrimary.vcf.gz";
secondary.tableFileUrl = "input/annoGrator/insertionsSecondary.vcf.gz";
doInsertionsRegions(&primary);
}


struct testSpec
    {
    char *name;
    void (*TestFunc)(struct annoAssembly *assembly);
    };

static const struct testSpec testSpecList[] =
{
    { "pgSnpDbToTabOut", pgSnpDbToTabOut },
    { "pgSnpKgDbToTabOutShort", pgSnpKgDbToTabOutShort },
    { "pgSnpKgDbToTabOutLong", pgSnpKgDbToTabOutLong },
    { "pgSnpKgDbToGpFx", pgSnpKgDbToGpFx },
    { "snpConsDbToTabOutShort", snpConsDbToTabOutShort },
    { "snpConsDbToTabOutLong", snpConsDbToTabOutLong },
    { "vcfEx1", vcfEx1 },
    { "vcfEx2", vcfEx2 },
    { "bigBedToTabOut", bigBedToTabOut },
    { "snpBigWigToTabOut", snpBigWigToTabOut },
    { "vepOut", vepOut },
    { "vepOutIndelTrim", vepOutIndelTrim },
    { "gpFx", gpFx },
    { "insertions", insertions },
    { NULL, NULL }
};

struct slName *makeTestNameList()
// Extract just the names of the tests into a list.
{
struct slName *testNameList = NULL;
int i;
for (i = 0;  testSpecList[i].name != NULL;  i++)
    {
    slAddHead(&testNameList, slNameNew(testSpecList[i].name));
    }
slReverse(&testNameList);
return testNameList;
}

char *makeTestNameUsage(struct slName *testNameList)
// Make a user-friendly listing of valid test names
{
struct dyString *dy = dyStringCreate("testName can be one of the following:\n");
struct slName *testName;
for (testName = testNameList;  testName != NULL;  testName = testName->next)
    {
    dyStringPrintf(dy, "    %s\n", testName->name);
    }
return dyStringCannibalize(&dy);
}

void usage(struct slName *testNameList)
/* explain usage and exit */
{
errAbort(
    "annoGratorTester - test program for anno* lib modules\n\n"
    "usage:\n"
    "    annoGratorTester db testName\n"
//    "options:\n"
   "%s", makeTestNameUsage(testNameList)
    );
}

static struct optionSpec optionSpecs[] = {
    {NULL, 0}
};

int main(int argc, char *argv[])
{
// Check args
optionInit(&argc, argv, optionSpecs);
struct slName *testNameList = makeTestNameList();
if (argc != 3)
    usage(testNameList);
char *db = argv[1];
char *testName = argv[2];
if (! slNameFind(testNameList, testName))
    {
    errAbort("Unrecognized test name '%s'\n"
             "%s", argv[2], makeTestNameUsage(testNameList));
    }

// Set up environment
pushCarefulMemHandler(LIMIT_2or6GB);
if (udcCacheTimeout() < 300)
    udcSetCacheTimeout(300);
udcSetDefaultDir("./udcCache");
knetUdcInstall();

// Run the specified test
struct annoAssembly *assembly = getAnnoAssembly(db);
int i;
for (i = 0;  testSpecList[i].name != NULL;  i++)
    {
    struct testSpec testSpec = testSpecList[i];
    if (sameString(testName, testSpec.name))
        testSpec.TestFunc(assembly);
    }

return 0;
}
