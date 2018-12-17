/* hAnno -- helpers for creating anno{Streamers,Grators,Formatters,Queries} */

#include "common.h"
#include "hAnno.h"
#include "basicBed.h"
#include "bigGenePred.h"
#include "customTrack.h"
#include "factorSource.h"
#include "grp.h"
#include "hdb.h"
#include "hubConnect.h"
#include "hui.h"
#include "jksql.h"
#include "pgSnp.h"
#include "trackHub.h"
#include "vcf.h"
#include "annoGratorQuery.h"
#include "annoGratorGpVar.h"
#include "annoStreamBigBed.h"
#include "annoStreamBigWig.h"
#include "annoStreamDb.h"
#include "annoStreamDbFactorSource.h"
#include "annoStreamDbPslPlus.h"
#include "annoStreamTab.h"
#include "annoStreamVcf.h"
#include "annoStreamLongTabix.h"
#include "annoStreamWig.h"
#include "annoGrateWigDb.h"
#include "annoFormatTab.h"
#include "annoFormatVep.h"

//#*** duplicated in hgVarAnnoGrator and annoGratorTester
struct annoAssembly *hAnnoGetAssembly(char *db)
/* Make annoAssembly for db. */
{
static struct annoAssembly *aa = NULL;
if (aa == NULL)
    {
    if (trackHubDatabase(db))
        {
        struct trackHubGenome *thg = trackHubGetGenome(db);
        char *url = thg->twoBitPath;
        aa = annoAssemblyNew(db, url);
        }
    else
        {
        char twoBitPath[HDB_MAX_PATH_STRING];
        safef(twoBitPath, sizeof(twoBitPath), "/gbdb/%s/%s.2bit", db, db);
        char *path = hReplaceGbdb(twoBitPath);
        aa = annoAssemblyNew(db, path);
        freeMem(path);
        }
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

static struct asObject *asObjectFromFields(char *name, struct sqlFieldInfo *fieldList,
                                           boolean skipBin)
/* Make autoSql text from SQL fields and pass it to asParse. */
{
struct dyString *dy = dyStringCreate("table %s\n"
				     "\"Column names grabbed from mysql\"\n"
				     "    (\n", name);
struct sqlFieldInfo *field;
for (field = fieldList;  field != NULL;  field = field->next)
    {
    if (skipBin && field == fieldList && sameString("bin", field->field))
        continue;
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

struct asObject *hAnnoGetAutoSqlForDbTable(char *db, char *table, struct trackDb *tdb,
                                           boolean skipBin)
/* Get autoSql for db.table from optional tdb and/or db.tableDescriptions;
 * if it doesn't match columns, make one up from db.table sql fields.
 * Some subtleties are lost in translation from .as to .sql, that's why
 * we try tdb & db.tableDescriptions first.  But ultimately we need to return
 * an asObj whose columns match all fields of the table. */
{
struct sqlConnection *conn = hAllocConn(db);
char maybeSplitTable[HDB_MAX_TABLE_STRING];
if (!hFindSplitTable(db, NULL, table, maybeSplitTable, sizeof maybeSplitTable, NULL))
    errAbort("hAnnoGetAutoSqlForDbTable: can't find table (or split table) for '%s.%s'", db, table);
struct sqlFieldInfo *fieldList = sqlFieldInfoGet(conn, maybeSplitTable);
struct asObject *asObj = NULL;
if (tdb != NULL)
    asObj = asForTdb(conn, tdb);
if (asObj == NULL)
    asObj = asFromTableDescriptions(conn, table);
hFreeConn(&conn);
if (columnsMatch(asObj, fieldList))
    return asObj;
else
    {
    // Special case for pgSnp, which includes its bin column in autoSql...
    struct asObject *pgSnpAsO = pgSnpAsObj();
    if (columnsMatch(pgSnpAsO, fieldList))
        return pgSnpAsO;
    return asObjectFromFields(table, fieldList, skipBin);
    }
}

static char *getBigDataIndexName(struct trackDb *tdb)
/* Get tbi/bai URL for a BAM/VCF from trackDb or custom track. */
{
char *bigIndexUrl = trackDbSetting(tdb, "bigDataIndex");
if (isNotEmpty(bigIndexUrl))
    return bigIndexUrl;
return NULL;
}

static char *getBigDataFileName(char *db, struct trackDb *tdb, char *selTable, char *chrom)
/* Get fileName from bigBed/bigWig/BAM/VCF database table, or bigDataUrl from custom track. */
{
char *bigDataUrl = trackDbSetting(tdb, "bigDataUrl");
if (isNotEmpty(bigDataUrl))
    {
    return bigDataUrl;
    }
else
    {
    struct sqlConnection *conn = hAllocConn(db);
    char *fileOrUrl = bbiNameFromSettingOrTableChrom(tdb, conn, selTable, chrom);
    hFreeConn(&conn);
    return fileOrUrl;
    }
}

static boolean dbTableMatchesAutoSql(char *db, char *table, struct asObject *asObj)
/* Return true if table exists and its fields match the columns of asObj. */
{
boolean matches = FALSE;
struct sqlConnection *conn = hAllocConn(db);
if (sqlTableExists(conn, table))
    {
    struct sqlFieldInfo *fieldList = sqlFieldInfoGet(conn, table);
    matches = columnsMatch(asObj, fieldList);
    }
hFreeConn(&conn);
return matches;
}

struct annoStreamer *hAnnoStreamerFromBigFileUrl(char *fileOrUrl, char *indexUrl, struct annoAssembly *assembly,
                                                 int maxOutRows, char *type)
/* Determine what kind of big data file/url we have and make streamer for it.
 * If type is NULL, this will determine type using custom track type or file suffix.
 * indexUrl can be NULL, unless the type is VCF and the .tbi file is not alongside the .VCF */
{
struct annoStreamer *streamer = NULL;
if (isEmpty(type))
    type = customTrackTypeFromBigFile(fileOrUrl);
if (type == NULL)
    {
    if (endsWith(fileOrUrl, "pgSnp") || endsWith(fileOrUrl, "pgsnp") ||
        endsWith(fileOrUrl, "pgSnp.gz") || endsWith(fileOrUrl, "pgsnp.gz") ||
        endsWith(fileOrUrl, "bed") || endsWith(fileOrUrl, "bed.gz"))
        {
        type = "pgSnp";
        }
    else
        errAbort("Unrecognized bigData type of file or url '%s'", fileOrUrl);
    }
if (startsWith("big", type))
    {
    if (startsWithWord("bigWig", type))
        streamer = annoStreamBigWigNew(fileOrUrl, assembly);
    else
        streamer = annoStreamBigBedNew(fileOrUrl, assembly, maxOutRows);
    }
else if (sameString(type, "vcfTabix"))
    streamer = annoStreamVcfNew(fileOrUrl, indexUrl, TRUE, assembly, maxOutRows);
else if (sameString(type, "vcf"))
    streamer = annoStreamVcfNew(fileOrUrl, NULL, FALSE, assembly, maxOutRows);
else if (sameString(type, "pgSnp"))
    streamer = annoStreamTabNew(fileOrUrl, assembly, pgSnpFileAsObj(), maxOutRows);
else if (sameString(type, "bam"))
    errAbort("Sorry, BAM is not yet supported");
else
    errAbort("Unrecognized bigData type %s of file or url '%s'", type, fileOrUrl);
return streamer;
}

struct annoStreamer *hAnnoStreamerFromTrackDb(struct annoAssembly *assembly, char *selTable,
                                              struct trackDb *tdb, char *chrom, int maxOutRows,
                                              struct jsonElement *config)
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
if (startsWithWord("wig", tdb->type))
    streamer = annoStreamWigDbNew(dataDb, dbTable, assembly, maxOutRows);
else if (sameString("longTabix", tdb->type))
    {
    char *fileOrUrl = getBigDataFileName(dataDb, tdb, selTable, chrom);
    streamer = annoStreamLongTabixNew(fileOrUrl,  assembly, maxOutRows);
    }
else if (sameString("vcfTabix", tdb->type))
    {
    char *fileOrUrl = getBigDataFileName(dataDb, tdb, selTable, chrom);
    char *indexUrl = getBigDataIndexName(tdb);
    streamer = annoStreamVcfNew(fileOrUrl, indexUrl, TRUE, assembly, maxOutRows);
    }
else if (sameString("vcf", tdb->type))
    {
    char *fileOrUrl = getBigDataFileName(dataDb, tdb, dbTable, chrom);
    streamer = annoStreamVcfNew(fileOrUrl, NULL, FALSE, assembly, maxOutRows);
    }
else if (sameString("bam", tdb->type))
    {
    warn("Sorry, BAM is not yet supported");
    }
else if (startsWith("big", tdb->type))
    {
    char *fileOrUrl = getBigDataFileName(dataDb, tdb, selTable, chrom);
    if (startsWithWord("bigWig", tdb->type))
        streamer = annoStreamBigWigNew(fileOrUrl, assembly); //#*** no maxOutRows support
    else
        streamer = annoStreamBigBedNew(fileOrUrl, assembly, maxOutRows);
    }
else if (sameString("factorSource", tdb->type) &&
         dbTableMatchesAutoSql(dataDb, tdb->table, factorSourceAsObj()))
    {
    char *sourceTable = trackDbSetting(tdb, "sourceTable");
    char *inputsTable = trackDbSetting(tdb, "inputTrackTable");
    streamer = annoStreamDbFactorSourceNew(dataDb, tdb->track, sourceTable, inputsTable, assembly,
					   maxOutRows);
    }
else if (trackHubDatabase(db) && !isCustomTrack(selTable))
    errAbort("Unrecognized type '%s' for hub track '%s'", tdb->type, tdb->track);
if (streamer == NULL)
    {
    streamer = annoStreamDbNew(dataDb, dbTable, assembly, maxOutRows, config);
    }
return streamer;
}

struct annoGrator *hAnnoGratorFromBigFileUrl(char *fileOrUrl, char *indexUrl, struct annoAssembly *assembly,
                                             int maxOutRows, enum annoGratorOverlap overlapRule)
/* Determine what kind of big data file/url we have and make streamer & grator for it. */
{
struct annoStreamer *streamer = NULL;
struct annoGrator *grator = NULL;
char *type = customTrackTypeFromBigFile(fileOrUrl);
if (startsWithWord("bigWig", type))
    grator = annoGrateBigWigNew(fileOrUrl, assembly, agwmAverage);
else if (startsWith("big", type))
    streamer = annoStreamBigBedNew(fileOrUrl, assembly, maxOutRows);
else if (sameString(type, "vcfTabix"))
    streamer = annoStreamVcfNew(fileOrUrl, indexUrl, TRUE, assembly, maxOutRows);
else if (sameString(type, "bam"))
    errAbort("Sorry, BAM is not yet supported");
else
    errAbort("Unrecognized bigData type %s of file or url '%s'", type, fileOrUrl);
if (grator == NULL)
    grator = annoGratorNew(streamer);
grator->setOverlapRule(grator, overlapRule);
return grator;
}

struct annoGrator *hAnnoGratorFromTrackDb(struct annoAssembly *assembly, char *selTable,
                                          struct trackDb *tdb, char *chrom, int maxOutRows,
                                          struct asObject *primaryAsObj,
                                          enum annoGratorOverlap overlapRule,
                                          struct jsonElement *config)
/* Figure out the source and type of data, make an annoStreamer & wrap in annoGrator.
 * If not NULL, primaryAsObj is used to determine whether we can make an annoGratorGpVar. */
{
struct annoGrator *grator = NULL;
boolean primaryIsVariants = (primaryAsObj != NULL &&
                             (asObjectsMatch(primaryAsObj, pgSnpAsObj()) ||
                              asObjectsMatch(primaryAsObj, pgSnpFileAsObj()) ||
                              asObjectsMatch(primaryAsObj, vcfAsObj())));
char *bigDataUrl = trackDbSetting(tdb, "bigDataUrl");
char *indexUrl = getBigDataIndexName(tdb);
if (bigDataUrl != NULL)
    {
    if (primaryIsVariants && sameString("bigGenePred", tdb->type))
        {
        struct annoStreamer *streamer = annoStreamBigBedNew(bigDataUrl, assembly, maxOutRows);
        grator = annoGratorGpVarNew(streamer);
        }
    else
        grator = hAnnoGratorFromBigFileUrl(bigDataUrl, indexUrl, assembly, maxOutRows, overlapRule);
    }
else if (startsWithWord("wig", tdb->type))
    {
    char *dataDb = assembly->name;
    char *dbTable = selTable;
    if (isCustomTrack(selTable))
        {
        dbTable = trackDbSetting(tdb, "dbTableName");
        if (dbTable != NULL)
            // This is really a database table, not a bigDataUrl CT.
            dataDb = CUSTOM_TRASH;
        }
    grator = annoGrateWigDbNew(dataDb, dbTable, assembly, agwmAverage, maxOutRows);
    }
else if (startsWithWord("bigWig", tdb->type))
    {
    char *fileOrUrl = getBigDataFileName(assembly->name, tdb, tdb->table, chrom);
    grator = annoGrateBigWigNew(fileOrUrl, assembly, agwmAverage);
    }
else
    {
    struct annoStreamer *streamer = hAnnoStreamerFromTrackDb(assembly, selTable, tdb, chrom,
                                                             maxOutRows, config);
    boolean streamerIsGenePred = asColumnNamesMatchFirstN(streamer->asObj, genePredAsObj(), 10);
    boolean streamerIsBigGenePred = asObjectsMatch(streamer->asObj, bigGenePredAsObj());
    if (primaryIsVariants && (streamerIsGenePred || streamerIsBigGenePred))
        {
        if (streamerIsGenePred &&
            (sameString("refGene", tdb->table) || startsWith("ncbiRefSeq", tdb->table)))
            {
            // We have PSL+CDS+seq for these tracks -- pass that instead of genePred
            // to annoGratorGpVar
            streamer->close(&streamer);
            streamer = annoStreamDbPslPlusNew(assembly, tdb->table, maxOutRows, config);
            }
	grator = annoGratorGpVarNew(streamer);
        }
    else
	grator = annoGratorNew(streamer);
    }
grator->setOverlapRule(grator, overlapRule);
return grator;
}

static struct asObject *getAutoSqlForType(char *db, char *chrom, struct trackDb *tdb)
/* Return an asObject for tdb->type if recognized as a hub or custom track type. */
{
struct asObject * asObj = NULL;
if (startsWith("wig", tdb->type) || startsWithWord("bigWig", tdb->type))
    asObj = annoStreamBigWigAsObject();
else if (startsWith("big", tdb->type))
    {
    char *fileOrUrl = getBigDataFileName(db, tdb, tdb->table, chrom);
    asObj = bigBedFileAsObjOrDefault(fileOrUrl);
    }
else if (startsWith("vcf", tdb->type))
    asObj = vcfAsObj();
else if (sameString("pgSnp", tdb->type))
    asObj = pgSnpAsObj();
else if (sameString("bam", tdb->type) || sameString("maf", tdb->type))
    warn("Sorry, %s is not yet supported", tdb->type);
else if (startsWithWord("bed", tdb->type) && !strchr(tdb->type, '+'))
    {
    // BED with no + fields; parse bed field count out of type line.
    int bedFieldCount = 3;
    char typeCopy[PATH_LEN];
    safecpy(typeCopy, sizeof(typeCopy), tdb->type);
    char *words[8];
    int wordCount = chopLine(typeCopy, words);
    if (wordCount > 1)
        bedFieldCount = atoi(words[1]);
    asObj = asParseText(bedAsDef(bedFieldCount, bedFieldCount));
    }
else if (sameString("factorSource", tdb->type) &&
         dbTableMatchesAutoSql(db, tdb->table, factorSourceAsObj()))
    {
    asObj = annoStreamDbFactorSourceAsObj();
    }
return asObj;
}

struct asObject *hAnnoGetAutoSqlForTdb(char *db, char *chrom, struct trackDb *tdb)
/* If possible, return the asObj that a streamer for this track would use, otherwise NULL. */
{
struct asObject *asObj = getAutoSqlForType(db, chrom, tdb);

if (!asObj && !isHubTrack(tdb->track))
    {
    // If none of the above, it must be a database table; deduce autoSql from sql fields.
    char *dataDb = db, *dbTable = tdb->table;
    if (isCustomTrack(tdb->track))
        {
        dbTable = trackDbSetting(tdb, "dbTableName");
        if (dbTable)
            dataDb = CUSTOM_TRASH;
        else
            return NULL;
        }
    asObj = hAnnoGetAutoSqlForDbTable(dataDb, dbTable, tdb, TRUE);
    }
return asObj;
}

