/* Stuff lifted from hgTables that should be libified. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "cheapcgi.h"
#include "customTrack.h"
#include "grp.h"
#include "hdb.h"
#include "hgFind.h"
#include "hubConnect.h"
#include "hui.h"
#include "trackHub.h"
#include "wikiTrack.h"
#include "annoGratorQuery.h"
#include "annoGratorGpVar.h"
#include "annoStreamBigBed.h"
#include "annoStreamDb.h"
#include "annoStreamDbFactorSource.h"
#include "annoStreamTab.h"
#include "annoStreamVcf.h"
#include "annoStreamWig.h"
#include "annoGrateWigDb.h"
#include "annoFormatTab.h"
#include "annoFormatVep.h"
#include "pgSnp.h"
#include "vcf.h"

#include "libifyMe.h"

static boolean searchPosition(char *range, struct cart *cart, char *cartVar)
/* Try and fill in region via call to hgFind. Return FALSE
 * if it can't find a single position. */
{
struct hgPositions *hgp = NULL;
char retAddr[512];
char position[512];
char *chrom = NULL;
int start=0, end=0;
char *db = cloneString(cartString(cart, "db")); // gets clobbered if position is not found!
safef(retAddr, sizeof(retAddr), "%s", cgiScriptName());
hgp = findGenomePosWeb(db, range, &chrom, &start, &end,
	cart, TRUE, retAddr);
if (hgp != NULL && hgp->singlePos != NULL)
    {
    safef(position, sizeof(position),
	    "%s:%d-%d", chrom, start+1, end);
    cartSetString(cart, cartVar, position);
    return TRUE;
    }
else if (start == 0)	/* Confusing way findGenomePosWeb says pos not found. */
    {
    cartSetString(cart, cartVar, hDefaultPos(db));
    return FALSE;
    }
else
    return FALSE;
}

boolean lookupPosition(struct cart *cart, char *cartVar)
/* Look up position if it is not already seq:start-end.  Return FALSE if it puts
 * up multiple positions. */
{
char *db = cartString(cart, "db");
char *range = cartUsualString(cart, cartVar, "");
boolean isSingle = TRUE;
range = trimSpaces(range);
if (range[0] != 0)
    isSingle = searchPosition(range, cart, cartVar);
else
    cartSetString(cart, cartVar, hDefaultPos(db));
return isSingle;
}

void nbSpaces(int count)
/* Print some non-breaking spaces. */
{
int i;
for (i=0; i<count; ++i)
    printf("&nbsp;");
}

//#*** duplicated in hgVarAnnoGrator and annoGratorTester
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

static boolean columnsMatch(struct asObject *asObj, struct sqlFieldInfo *fieldList)
/* Return TRUE if asObj's column names match the given SQL fields. */
{
if (asObj == NULL)
    return FALSE;
struct sqlFieldInfo *firstRealField = fieldList;
if (sameString("bin", fieldList->field) && differentString("bin", asObj->columnList->name))
    firstRealField = fieldList->next;
boolean columnsMatch = TRUE;
struct sqlFieldInfo *field = firstRealField;
struct asColumn *asCol = asObj->columnList;
for (;  field != NULL && asCol != NULL;  field = field->next, asCol = asCol->next)
    {
    if (!sameString(field->field, asCol->name))
	{
	columnsMatch = FALSE;
	break;
	}
    }
if (field != NULL || asCol != NULL)
    columnsMatch = FALSE;
return columnsMatch;
}

static struct asObject *asObjectFromFields(char *name, struct sqlFieldInfo *fieldList)
/* Make autoSql text from SQL fields and pass it to asParse. */
{
struct dyString *dy = dyStringCreate("table %s\n"
				     "\"Column names grabbed from mysql\"\n"
				     "    (\n", name);
struct sqlFieldInfo *field;
for (field = fieldList;  field != NULL;  field = field->next)
    {
    char *sqlType = field->type;
    // hg19.wgEncodeOpenChromSynthGm12878Pk.pValue has sql type "float unsigned",
    // and I'd rather pretend it's just a float than work unsigned floats into autoSql.
    if (sameString(sqlType, "float unsigned"))
	sqlType = "float";
    char *asType = asTypeNameFromSqlType(sqlType);
    if (asType == NULL)
	errAbort("No asTypeInfo for sql type '%s'!", field->type);
    dyStringPrintf(dy, "    %s %s;\t\"\"\n", asType, field->field);
    }
dyStringAppend(dy, "    )\n");
return asParseText(dy->string);
}

static struct asObject *getAutoSqlForTable(char *db, char *dataDb, char *dbTable,
					   struct trackDb *tdb)
/* Get autoSql for dataDb.dbTable from tdb and/or db.tableDescriptions;
 * if it doesn't match columns, make one up from dataDb.table sql fields. */
//#*** should we just always use sql fields?
{
struct sqlConnection *connDataDb = hAllocConn(dataDb);
struct sqlFieldInfo *fieldList = sqlFieldInfoGet(connDataDb, dbTable);
hFreeConn(&connDataDb);
struct asObject *asObj = NULL;
if (tdb != NULL)
    {
    struct sqlConnection *connDb = hAllocConn(db);
    asObj = asForTdb(connDb, tdb);
    hFreeConn(&connDb);
    }
if (columnsMatch(asObj, fieldList))
    return asObj;
else
    return asObjectFromFields(dbTable, fieldList);
}

static char *getBigDataFileName(char *db, struct trackDb *tdb, char *selTable, char *chrom)
/* Get fileName from bigBed/bigWig/BAM/VCF database table, or bigDataUrl from custom track. */
{
struct sqlConnection *conn = hAllocConn(db);
char *fileOrUrl = bbiNameFromSettingOrTableChrom(tdb, conn, selTable, chrom);
hFreeConn(&conn);
return fileOrUrl;
}

struct annoStreamer *streamerFromTrack(struct annoAssembly *assembly, char *selTable,
				       struct trackDb *tdb, char *chrom, int maxOutRows)
/* Figure out the source and type of data and make an annoStreamer. */
{
struct annoStreamer *streamer = NULL;
char *db = assembly->name, *dataDb = db, *dbTable = selTable;
if (chrom == NULL)
    chrom = hDefaultChrom(db);
if (isCustomTrack(selTable))
    {
    dbTable = trackDbSetting(tdb, "dbTableName");
    if (dbTable != NULL)
	// This is really a database table, not a bigDataUrl CT.
	dataDb = CUSTOM_TRASH;
    }
if (startsWith("wig", tdb->type))
    streamer = annoStreamWigDbNew(dataDb, dbTable, assembly, maxOutRows);
else if (sameString("vcfTabix", tdb->type))
    {
    char *fileOrUrl = getBigDataFileName(db, tdb, selTable, chrom);
    streamer = annoStreamVcfNew(fileOrUrl, TRUE, assembly, maxOutRows);
    }
else if (sameString("vcf", tdb->type))
    {
    char *fileOrUrl = getBigDataFileName(dataDb, tdb, dbTable, chrom);
    streamer = annoStreamVcfNew(fileOrUrl, FALSE, assembly, maxOutRows);
    }
else if (sameString("bam", tdb->type))
    {
    warn("Sorry, BAM is not yet supported");
    }
else if (startsWith("bigBed", tdb->type))
    {
    char *fileOrUrl = getBigDataFileName(db, tdb, selTable, chrom);
    streamer = annoStreamBigBedNew(fileOrUrl, assembly, maxOutRows);
    }
else if (sameString("factorSource", tdb->type))
    {
    char *sourceTable = trackDbSetting(tdb, "sourceTable");
    char *inputsTable = trackDbSetting(tdb, "inputTrackTable");
    streamer = annoStreamDbFactorSourceNew(db, tdb->track, sourceTable, inputsTable, assembly,
					   maxOutRows);
    }
else
    {
    struct sqlConnection *conn = hAllocConn(dataDb);
    char maybeSplitTable[1024];
    if (sqlTableExists(conn, dbTable))
	safecpy(maybeSplitTable, sizeof(maybeSplitTable), dbTable);
    else
	safef(maybeSplitTable, sizeof(maybeSplitTable), "%s_%s", chrom, dbTable);
    hFreeConn(&conn);
    struct asObject *asObj = getAutoSqlForTable(db, dataDb, maybeSplitTable, tdb);
    streamer = annoStreamDbNew(dataDb, maybeSplitTable, assembly, asObj, maxOutRows);
    }
return streamer;
}

struct annoGrator *gratorFromBigDataFileOrUrl(char *fileOrUrl, struct annoAssembly *assembly,
					      int maxOutRows, enum annoGratorOverlap overlapRule)
/* Determine what kind of big data file/url we have and make streamer & grator for it. */
{
struct annoStreamer *streamer = NULL;
struct annoGrator *grator = NULL;
if (endsWith(fileOrUrl, ".bb"))
    streamer = annoStreamBigBedNew(fileOrUrl, assembly, maxOutRows);
else if (endsWith(fileOrUrl, ".vcf.gz"))
    streamer = annoStreamVcfNew(fileOrUrl, TRUE, assembly, maxOutRows);
else if (endsWith(fileOrUrl, ".bw"))
    grator = annoGrateBigWigNew(fileOrUrl, assembly);
else
    errAbort("Can't tell bigData type of file or url '%s'", fileOrUrl);
if (grator == NULL)
    grator = annoGratorNew(streamer);
grator->setOverlapRule(grator, overlapRule);
return grator;
}

struct annoGrator *gratorFromTrackDb(struct annoAssembly *assembly, char *selTable,
				     struct trackDb *tdb, char *chrom, int maxOutRows,
				     struct asObject *primaryAsObj,
				     enum annoGratorOverlap overlapRule)
/* Figure out the source and type of data, make an annoStreamer & wrap in annoGrator.
 * If not NULL, primaryAsObj is used to determine whether we can make an annoGratorGpVar. */
{
struct annoGrator *grator = NULL;
char *dataDb = assembly->name, *dbTable = selTable;
if (isCustomTrack(selTable))
    {
    dataDb = CUSTOM_TRASH;
    dbTable = trackDbSetting(tdb, "dbTableName");
    if (dbTable == NULL)
	{
	char *bigDataUrl = trackDbSetting(tdb, "bigDataUrl");
	if (bigDataUrl != NULL)
	    grator = gratorFromBigDataFileOrUrl(bigDataUrl, assembly, maxOutRows, overlapRule);
	else
	    errAbort("Can't find dbTableName or bigDataUrl for custom track %s", selTable);
	}
    }
if (startsWith("wig", tdb->type))
    {
    grator = annoGrateWigDbNew(dataDb, dbTable, assembly, maxOutRows);
    }
else
    {
    struct annoStreamer *streamer = streamerFromTrack(assembly, dbTable, tdb, chrom, maxOutRows);
    if (primaryAsObj != NULL &&
	(asObjectsMatch(primaryAsObj, pgSnpAsObj()) || asObjectsMatch(primaryAsObj, vcfAsObj()))
	&& asColumnNamesMatchFirstN(streamer->asObj, genePredAsObj(), 10))
	grator = annoGratorGpVarNew(streamer);
    else
	grator = annoGratorNew(streamer);
    }
grator->setOverlapRule(grator, overlapRule);
return grator;
}

