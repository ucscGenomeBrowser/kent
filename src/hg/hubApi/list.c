/* manage endpoint /list/ functions */

#include "dataApi.h"
#include "bamFile.h"
#include "htslib/tbx.h"

static void hubPublicJsonData(struct jsonWrite *jw, struct hubPublic *el,
  int columnCount, char **columnNames)
/* Print array data for one row from hubPublic table, order here
 * must be same as was stated in the columnName header element
 * This code should be in hg/lib/hubPublic.c (which does not exist)
 */
{
int i = 0;
jsonWriteObjectStart(jw, NULL);
jsonWriteString(jw, columnNames[i++], el->hubUrl);
jsonWriteString(jw, columnNames[i++], el->shortLabel);
jsonWriteString(jw, columnNames[i++], el->longLabel);
jsonWriteString(jw, columnNames[i++], el->registrationTime);
jsonWriteNumber(jw, columnNames[i++], (long long)el->dbCount);
jsonWriteString(jw, columnNames[i++], el->dbList);
jsonWriteString(jw, columnNames[i++], el->descriptionUrl);
jsonWriteObjectEnd(jw);
}

static void jsonPublicHubs()
/* output the hubPublic SQL table */
{
char *extraArgs = verifyLegalArgs(argListPublicHubs); /* no extras allowed */
if (extraArgs)
    apiErrAbort(err400, err400Msg, "extraneous arguments found for function /list/publicHubs '%s'", extraArgs);

struct sqlConnection *conn = hConnectCentral();
char *dataTime = sqlTableUpdate(conn, hubPublicTableName());
time_t dataTimeStamp = sqlDateToUnixTime(dataTime);
replaceChar(dataTime, ' ', 'T');	/* ISO 8601 */
struct hubPublic *el = hubPublicDbLoadAll();
struct jsonWrite *jw = apiStartOutput();
jsonWriteString(jw, "dataTime", dataTime);
jsonWriteNumber(jw, "dataTimeStamp", (long long)dataTimeStamp);
freeMem(dataTime);
// redundant: jsonWriteString(jw, "tableName", hubPublicTableName());
char **columnNames = NULL;
char **columnTypes = NULL;
int *jsonTypes = NULL;
int columnCount = tableColumns(conn, hubPublicTableName(), &columnNames,
    &columnTypes, &jsonTypes);
jsonWriteListStart(jw, "publicHubs");
for ( ; el != NULL; el = el->next )
    {
    hubPublicJsonData(jw, el, columnCount, columnNames);
    }
jsonWriteListEnd(jw);
apiFinishOutput(0, NULL, jw);
hDisconnectCentral(&conn);
}

static void genArkJsonData(struct jsonWrite *jw, struct genark *el,
    int columnCount, char **columnNames)
/* Print out genark table element in JSON format.
 * must be same as was stated in the columnName header element
 */
{
int i = 0;
jsonWriteObjectStart(jw, el->gcAccession);
i++;
// redundant: jsonWriteString(jw, NULL, el->gcAccession);
jsonWriteString(jw, columnNames[i++], el->hubUrl);
jsonWriteString(jw, columnNames[i++], el->asmName);
jsonWriteString(jw, columnNames[i++], el->scientificName);
jsonWriteString(jw, columnNames[i++], el->commonName);
jsonWriteNumber(jw, columnNames[i++], (long long)el->taxId);
if (columnCount > 6)
    jsonWriteNumber(jw, columnNames[i++], (long long)el->priority);
if (columnCount > 7)
    jsonWriteString(jw, columnNames[i++], el->clade);
jsonWriteObjectEnd(jw);
}

static void dbDbJsonData(struct jsonWrite *jw, struct dbDb *el,
    int columnCount, char **columnNames)
/* Print out dbDb table element in JSON format.
 * must be same as was stated in the columnName header element
 * This code should be over in hg/lib/dbDb.c
 */
{
int i = 0;
jsonWriteObjectStart(jw, el->name);
i++;
// redundant: jsonWriteString(jw, NULL, el->name);
jsonWriteString(jw, columnNames[i++], el->description);
jsonWriteString(jw, columnNames[i++], el->nibPath);
jsonWriteString(jw, columnNames[i++], el->organism);
jsonWriteString(jw, columnNames[i++], el->defaultPos);
jsonWriteNumber(jw, columnNames[i++], (long long)el->active);
jsonWriteNumber(jw, columnNames[i++], (long long)el->orderKey);
jsonWriteString(jw, columnNames[i++], el->genome);
jsonWriteString(jw, columnNames[i++], el->scientificName);
jsonWriteString(jw, columnNames[i++], el->htmlPath);
jsonWriteNumber(jw, columnNames[i++], (long long)el->hgNearOk);
jsonWriteNumber(jw, columnNames[i++], (long long)el->hgPbOk);
jsonWriteString(jw, columnNames[i++], el->sourceName);
jsonWriteNumber(jw, columnNames[i++], (long long)el->taxId);
jsonWriteObjectEnd(jw);
}

static void jsonDbDb()
/* output the dbDb SQL table */
{
char *extraArgs = verifyLegalArgs(argListUcscGenomes); /* no extras allowed */
if (extraArgs)
    apiErrAbort(err400, err400Msg, "extraneous arguments found for function /list/ucscGenomes '%s'", extraArgs);

struct sqlConnection *conn = hConnectCentral();
char *dataTime = sqlTableUpdate(conn, "dbDb");
time_t dataTimeStamp = sqlDateToUnixTime(dataTime);
replaceChar(dataTime, ' ', 'T');	/* ISO 8601 */
struct dbDb *dbList = ucscDbDb();
struct dbDb *el;
struct jsonWrite *jw = apiStartOutput();
jsonWriteString(jw, "dataTime", dataTime);
jsonWriteNumber(jw, "dataTimeStamp", (long long)dataTimeStamp);
freeMem(dataTime);
char **columnNames = NULL;
char **columnTypes = NULL;
int *jsonTypes = NULL;
int columnCount = tableColumns(conn, "dbDb", &columnNames, &columnTypes,
    &jsonTypes);
jsonWriteObjectStart(jw, "ucscGenomes");
for ( el=dbList; el != NULL; el = el->next )
    {
    dbDbJsonData(jw, el, columnCount, columnNames);
    }
jsonWriteObjectEnd(jw);
apiFinishOutput(0, NULL, jw);
hDisconnectCentral(&conn);
}

static void jsonGenArk()
/* output the genark.hgcentral SQL table */
{
char *extraArgs = verifyLegalArgs(argListGenarkGenomes); /* no extras allowed */
if (extraArgs)
    apiErrAbort(err400, err400Msg, "extraneous arguments found for function /list/genark '%s'", extraArgs);

struct sqlConnection *conn = hConnectCentral();
char *genArkTable = genarkTableName();
char *dataTime = sqlTableUpdate(conn, genArkTable);
time_t dataTimeStamp = sqlDateToUnixTime(dataTime);
replaceChar(dataTime, ' ', 'T');	/* ISO 8601 */
long long genArkCount = genArkSize();
char *genome = cgiOptionalString("genome");
struct genark *list = genArkList(genome);
struct jsonWrite *jw = apiStartOutput();
if (genome && slCount(list) < 1)
    apiErrAbort(err404, err404Msg, "genome '%s' not found", genome);
jsonWriteString(jw, "dataTime", dataTime);
jsonWriteNumber(jw, "dataTimeStamp", (long long)dataTimeStamp);
char *hubPrefix = cfgOption("genarkHubPrefix");
jsonWriteString(jw, "hubUrlPrefix", hubPrefix);
freeMem(dataTime);
char **columnNames = NULL;
char **columnTypes = NULL;
int *jsonTypes = NULL;
int columnCount = tableColumns(conn, genArkTable, &columnNames, &columnTypes,
    &jsonTypes);
jsonWriteObjectStart(jw, "genarkGenomes");
struct genark *el;
for ( el=list; el != NULL; el = el->next )
    {
    genArkJsonData(jw, el, columnCount, columnNames);
    }
jsonWriteObjectEnd(jw);
long long itemCount = slCount(list);
jsonWriteNumber(jw, "totalAssemblies", genArkCount);
jsonWriteNumber(jw, "itemsReturned", itemCount);
if (!genome && (genArkCount > itemCount))
    jsonWriteBoolean(jw, "maxItemsLimit", TRUE);
apiFinishOutput(0, NULL, jw);
hDisconnectCentral(&conn);
}

static void bigFileChromInfoOutput(struct jsonWrite *jw,
    struct trackDb *thisTrack, char *bigDataUrl)
/* output the chromosome list for the bigDataUrl file */
{
struct bbiFile *bbi = bigFileOpen(thisTrack->type, bigDataUrl);
struct bbiChromInfo *chrList = bbiChromList(bbi);
slSort(chrList, chromInfoCmp);
struct bbiChromInfo *el = chrList;
jsonWriteNumber(jw, "chromCount", (long long)slCount(chrList));
jsonWriteObjectStart(jw, "chromosomes");
for ( ; el; el = el->next )
    {
    jsonWriteNumber(jw, el->name, (long long)el->size);
    }
jsonWriteObjectEnd(jw);	/* chromosomes */
}

static void hubChromInfoJsonOutput(FILE *f, char *hubUrl, char *genome)
/* for given hubUrl list the chromosomes in the sequence for specified genome
 */
{
struct trackHub *hub = errCatchTrackHubOpen(hubUrl);
struct trackHubGenome *ge = NULL;
char *track = cgiOptionalString("track");

if (isEmpty(genome))
    apiErrAbort(err400, err400Msg, "must specify a 'genome=name' with hubUrl for endpoint: /list/chromosomes?hubUrl=%s;genome=<empty>", hubUrl);

struct trackHubGenome *foundGenome = NULL;

for (ge = hub->genomeList; ge; ge = ge->next)
    {
    if (sameOk(genome, ge->name))
	{
	foundGenome = ge;
	continue;	/* found genome */
	}
    }

if (NULL == foundGenome)
    apiErrAbort(err400, err400Msg, "can not find specified 'genome=%s' for endpoint: /list/chromosomes?hubUrl=%s;genome=%s", genome, hubUrl, genome);

struct jsonWrite *jw = apiStartOutput();
jsonWriteString(jw, "hubUrl", hubUrl);
jsonWriteString(jw, "genome", genome);
if (isNotEmpty(track))
    {
    jsonWriteString(jw, "track", track);
    struct trackDb *tdb = obtainTdb(foundGenome, NULL);
    if (NULL == tdb)
	apiErrAbort(err400, err400Msg, "failed to find a track hub definition in genome=%s for endpoint '/list/chromosomes' given hubUrl=%s'", genome, hubUrl);

    struct trackDb *thisTrack = findTrackDb(track, tdb);
    if (NULL == thisTrack)
	apiErrAbort(err400, err400Msg, "failed to find specified track=%s in genome=%s for endpoint '/list/chromosomes'  given hubUrl='%s'", track, genome, hubUrl);

    char *bigDataUrl = trackDbSetting(thisTrack, "bigDataUrl");
    bigFileChromInfoOutput(jw, thisTrack, bigDataUrl);
    }
else
    {
    struct chromInfo *ci = NULL;
    /* might be a track hub on a UCSC database */
    if (isEmpty(foundGenome->twoBitPath))
	{
	struct sqlConnection *conn = hAllocConnMaybe(foundGenome->trackHub->defaultDb);
	if (NULL == conn)
	    apiErrAbort(err400, err400Msg, "can not find 'genome=%s' for endpoint '/list/chromosomes", foundGenome->trackHub->defaultDb);
	else
	    hFreeConn(&conn);
	ci = createChromInfoList(NULL, foundGenome->trackHub->defaultDb);
	}
    else
	{
	ci = trackHubAllChromInfo(foundGenome->name);
	}
    slSort(ci, chromInfoCmp);
    jsonWriteNumber(jw, "chromCount", (long long)slCount(ci));
    jsonWriteObjectStart(jw, "chromosomes");
    struct chromInfo *el = ci;
    for ( ; el != NULL; el = el->next )
	{
	jsonWriteNumber(jw, el->chrom, (long long)el->size);
	}
    jsonWriteObjectEnd(jw);	/* chromosomes */
    }
apiFinishOutput(0, NULL, jw);
}

static char *validChromName(struct sqlConnection *conn, char *db, char *table,
   char **splitTableName, struct hTableInfo **tableInfo)
/* determine what the 'chrom' name should be for this table (aka track)
 * this function could be used in getData() also TBD
 */
{
static char *returnChrom = NULL;
/* to be determined if this table name is used or is some other name */
char *sqlTableName = cloneString(table);

/* 'track' name in trackDb usually refers to a SQL 'table' */
struct trackDb *tdb = obtainTdb(NULL, db);
struct trackDb *thisTrack = findTrackDb(table,tdb);

/* thisTrack can be NULL at this time, taken care of later */

if (trackHasNoData(thisTrack))
    apiErrAbort(err400, err400Msg, "container track '%s' does not contain data, use the children of this container for data access", table);
if (thisTrack && ! isSupportedType(thisTrack->type))
    apiErrAbort(err415, err415Msg, "track type '%s' for track=%s not supported at this time", thisTrack->type, table);

/* however, the trackDb might have a specific table defined instead */
char *tableName = trackDbSetting(thisTrack, "table");
if (isNotEmpty(tableName))
    {
    freeMem(sqlTableName);
    sqlTableName = cloneString(tableName);
    }

/* this function knows how to deal with split chromosomes, the NULL
 * here for the chrom name means to use the first chrom name in chromInfo
 */
struct hTableInfo *hti = hFindTableInfoWithConn(conn, NULL, sqlTableName);
*tableInfo = hti;	/* returning to caller */
/* check if table name needs to be modified */
if (hti && hti->isSplit)
    {
    char *defaultChrom = hDefaultChrom(db);
    char fullTableName[256];
    safef(fullTableName, sizeof(fullTableName), "%s_%s", defaultChrom, hti->rootName);
    freeMem(sqlTableName);
    sqlTableName = cloneString(fullTableName);
    *splitTableName = cloneString(fullTableName);	/* return to caller */
    }
else
    {
    *splitTableName = sqlTableName;	/* return to caller */
    }

if (! sqlTableExists(conn, sqlTableName))
    returnChrom = NULL;
/* may need to extend this in the future for other track types */
else if (sqlColumnExists(conn, sqlTableName, "chrom"))	/* standard bed tables */
    returnChrom = cloneString("chrom");
else if (sqlColumnExists(conn, sqlTableName, "tName"))	/* track type psl */
    returnChrom = cloneString("tName");
else if (sqlColumnExists(conn, sqlTableName, "genoName"))	/* track type rmsk */
    returnChrom = cloneString("genoName");

return returnChrom;
}	/*	static char *validChromName() */

static long long bbiItemCount(char *bigDataUrl, char *type, char *indexFileOrUrl)
/* check the bigDataUrl to see what the itemCount is there */
{
long long itemCount = 0;
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    if (allowedBigBedType(type))
        {
        struct bbiFile *bbi = NULL;
        bbi = bigBedFileOpen(bigDataUrl);
        itemCount = bigBedItemCount(bbi);
        bbiFileClose(&bbi);
        }
    else if (startsWithWord("bigWig", type))
        {
        struct bbiFile *bwf = bigWigFileOpen(bigDataUrl);
        struct bbiSummaryElement sum = bbiTotalSummary(bwf);
        itemCount = sum.validCount;
        bbiFileClose(&bwf);
        }
    else if (sameString("bam", type))
        {
        itemCount = bamFileItemCount(bigDataUrl, indexFileOrUrl);
        }
    else if (sameString("vcfTabix", type))
        {
        itemCount = vcfTabixItemCount(bigDataUrl, indexFileOrUrl);
        }
    }
errCatchEnd(errCatch);
if (isNotEmpty(errCatch->message->string))
    fprintf(stderr, "%s", errCatch->message->string);
errCatchFree(&errCatch);
return itemCount;
}

static void outputTrackDbVars(struct jsonWrite *jw, char *db, struct trackDb *tdb,
    long long itemCount)
/* JSON output the fundamental trackDb variables */
{
if (NULL == tdb)	/* might not be any trackDb */
    return;

boolean isContainer = tdbIsComposite(tdb) || tdbIsCompositeView(tdb);

boolean protectedData = FALSE;
protectedData = protectedTrack(db, tdb, tdb->track);
jsonWriteString(jw, "shortLabel", tdb->shortLabel);
jsonWriteString(jw, "type", tdb->type);
jsonWriteString(jw, "longLabel", tdb->longLabel);
if (! isContainer && (itemCount > 0))	/* containers do not have items to count and if itemCount == -1 we didn't count */
    jsonWriteNumber(jw, "itemCount", itemCount);
if (tdb->parent)
    {
    jsonWriteString(jw, "parent", tdb->parent->track);
    if (tdb->parent->parent)
        jsonWriteString(jw, "parentParent", tdb->parent->parent->track);
    }
if (tdb->settingsHash)
    {
    struct hashEl *hel;
    struct hashCookie hc = hashFirst(tdb->settingsHash);
    while ((hel = hashNext(&hc)) != NULL)
        {
        if (sameWord("track", hel->name))
            continue;	// already output in header
        if (sameWord("tableBrowser", hel->name)
		&& startsWithWord("off", (char*)hel->val))
            jsonWriteBoolean(jw, "protectedData", TRUE);
        else if (isEmpty((char *)hel->val))
            jsonWriteString(jw, hel->name, "empty");
        else if (protectedData && sameWord(hel->name, "bigDataUrl"))
            jsonWriteString(jw, hel->name, "protectedData");
        else
            jsonWriteString(jw, hel->name, (char *)hel->val);
        }
    }
}

static void hubSchemaJsonOutput(FILE *f, char *hubUrl, char *genome, char *track)
/* for given hubUrl and track, output the schema for the hub track */
{
struct trackHub *hub = errCatchTrackHubOpen(hubUrl);
struct trackHubGenome *ge = NULL;

if (isEmpty(genome))
    apiErrAbort(err400, err400Msg, "must specify a 'genome=name' with hubUrl for endpoint: /list/schema?hubUrl=%s;genome=<empty>", hubUrl);

struct trackHubGenome *foundGenome = NULL;

for (ge = hub->genomeList; ge; ge = ge->next)
    {
    if (sameOk(genome, ge->name))
	{
	foundGenome = ge;
	continue;	/* found genome */
	}
    }

if (NULL == foundGenome)
    apiErrAbort(err400, err400Msg, "can not find specified 'genome=%s' for endpoint: /list/schema?hubUrl=%s;genome=%s", genome, hubUrl, genome);

struct jsonWrite *jw = apiStartOutput();
jsonWriteString(jw, "hubUrl", hubUrl);
jsonWriteString(jw, "genome", genome);
jsonWriteString(jw, "track", track);

struct trackDb *tdb = obtainTdb(foundGenome, NULL);
if (NULL == tdb)
    apiErrAbort(err400, err400Msg, "failed to find a track hub definition in genome=%s track=%s for endpoint '/list/schema' given hubUrl=%s'", genome, track, hubUrl);

struct trackDb *thisTrack = findTrackDb(track, tdb);
if (NULL == thisTrack)
    apiErrAbort(err400, err400Msg, "failed to find specified track=%s in genome=%s for endpoint '/list/schema'  given hubUrl='%s'", track, genome, hubUrl);

char *bigDataUrl = hReplaceGbdb(trackDbSetting(thisTrack, "bigDataUrl"));
if (NULL == bigDataUrl)
    apiErrAbort(err400, err400Msg, "failed to find bigDataUrl for specified track=%s in genome=%s for endpoint '/list/schema'  given hubUrl='%s'", track, genome, hubUrl);
char *indexFileOrUrl = hReplaceGbdb(trackDbSetting(tdb, "bigDataIndex"));
struct bbiFile *bbi = bigFileOpen(thisTrack->type, bigDataUrl);
long long itemCount = bbiItemCount(bigDataUrl, thisTrack->type, indexFileOrUrl);

outputTrackDbVars(jw, genome, thisTrack, itemCount);

struct asObject *as = bigBedAsOrDefault(bbi);
if (! as)
    apiErrAbort(err500, err500Msg, "can not find schema definition for bigDataUrl '%s', track=%s genome: '%s' for endpoint '/list/schema' given hubUrl='%s'", bigDataUrl, track, genome, hubUrl);
struct sqlFieldType *fiList = sqlFieldTypesFromAs(as);
bigColumnTypes(jw, fiList, as);

apiFinishOutput(0, NULL, jw);
}	/* static void hubSchemaJsonOutput(FILE *f, char *hubUrl,
	 *	char *genome, char *track) */

static char *bigDataUrlFromTable(struct sqlConnection *conn, char *table)
/* perhaps there is a bigDataUrl in a database table */
{
char *bigDataUrl = NULL;
char query[4096];
char quickReturn[2048];

if (sqlColumnExists(conn, table, "fileName"))
    {
    sqlSafef(query, sizeof(query), "select fileName from %s", table);
    if (sqlQuickQuery(conn, query, quickReturn, sizeof(quickReturn)))
	bigDataUrl = hReplaceGbdb(cloneString(quickReturn));
    }

return bigDataUrl;
}

static void schemaJsonOutput(FILE *f, char *db, char *track)
/* for given db and track, output the schema for the associated table */
{
struct sqlConnection *conn = hAllocConnMaybe(db);
if (NULL == conn)
    apiErrAbort(err400, err400Msg, "can not find 'genome=%s' for endpoint '/list/schema", db);

struct trackDb *tdb = obtainTdb(NULL, db);
struct trackDb *thisTrack = findTrackDb(track, tdb);
if (NULL == thisTrack)	/* OK to work with tables without trackDb definitions */
    {
    if (! sqlTableExists(conn, track))
	apiErrAbort(err400, err400Msg, "failed to find specified track=%s in genome=%s for endpoint '/list/schema'", track, db);
    }
else if ( ! isSupportedType(thisTrack->type))
    apiErrAbort(err415, err415Msg, "track type '%s' for track=%s not supported at this time", thisTrack->type, track);

if (trackHasNoData(thisTrack))
    apiErrAbort(err400, err400Msg, "container track '%s' does not contain data, use the children of this container for data access", track);

/* might be a table that points to a big* file
 * or is just a bigDataUrl without any table
 */
char *bigDataUrl = trackDbSetting(thisTrack, "bigDataUrl");

char *sqlTableName = cloneString(track);
/* the trackDb might have a specific table defined instead */
char *tableName = trackDbSetting(thisTrack, "table");
if (isNotEmpty(tableName))
    {
    freeMem(sqlTableName);
    sqlTableName = cloneString(tableName);
    }

/* this function knows how to deal with split chromosomes, the NULL
 * here for the chrom name means to use the first chrom name in chromInfo
 */
struct hTableInfo *hti = hFindTableInfoWithConn(conn, NULL, sqlTableName);
/* check if table name needs to be modified */
char *splitTableName = NULL;
if (hti && hti->isSplit)
    {
    char *defaultChrom = hDefaultChrom(db);
    char fullTableName[256];
    safef(fullTableName, sizeof(fullTableName), "%s_%s", defaultChrom, hti->rootName);
    freeMem(sqlTableName);
    sqlTableName = cloneString(fullTableName);
    splitTableName = cloneString(fullTableName);
    }
else
    {
    splitTableName = sqlTableName;
    }

struct bbiFile *bbi = NULL;
if (thisTrack && startsWith("big", thisTrack->type))
    {
    if (isEmpty(bigDataUrl))
        bigDataUrl = bigDataUrlFromTable(conn, splitTableName);
    if (bigDataUrl)
	bbi = bigFileOpen(thisTrack->type, bigDataUrl);
    if (NULL == bbi)
	apiErrAbort(err400, err400Msg, "failed to find bigDataUrl=%s for track=%s type=%s in database=%s for endpoint '/list/schema'", bigDataUrl, track, thisTrack->type, db);
    }

struct jsonWrite *jw = apiStartOutput();
jsonWriteString(jw, "genome", db);
jsonWriteString(jw, "track", track);

time_t dataTimeStamp = 0;
char *dataTime = NULL;

if (bbi)
    {
    dataTimeStamp = bbiUpdateTime(bbi);
    dataTime = sqlUnixTimeToDate(&dataTimeStamp, FALSE);
    }
else
    {
    dataTime = sqlTableUpdate(conn, splitTableName);
    dataTimeStamp = sqlDateToUnixTime(dataTime);
    }

replaceChar(dataTime, ' ', 'T');	/* ISO 8601 */
jsonWriteString(jw, "dataTime", dataTime);
jsonWriteNumber(jw, "dataTimeStamp", (long long)dataTimeStamp);

char **columnNames = NULL;
char **columnTypes = NULL;
int *jsonTypes = NULL;
int columnCount = 0;
struct asObject *as = NULL;
struct asColumn *columnEl = NULL;
int asColumnCount = 0;
long long itemCount = 0;

if (bbi)
    {
    /* do not show itemCount for protected data */
    if (! protectedTrack(db, thisTrack, track))
	{
	char *indexFileOrUrl = hReplaceGbdb(trackDbSetting(thisTrack, "bigDataIndex"));
	itemCount = bbiItemCount(bigDataUrl, thisTrack->type, indexFileOrUrl);
	}
    if (startsWith("bigWig", thisTrack->type))
	{
	wigColumnTypes(jw);
	}
    else
	{
	as = bigBedAsOrDefault(bbi);
	if (! as)
	    apiErrAbort(err500, err500Msg, "can not find schema definition for bigDataUrl '%s', track=%s genome: '%s' for endpoint '/list/schema'", bigDataUrl, track, db);
	struct sqlFieldType *fiList = sqlFieldTypesFromAs(as);
	bigColumnTypes(jw, fiList, as);
	}
    }
else
    {
    columnCount = tableColumns(conn, splitTableName, &columnNames, &columnTypes, &jsonTypes);
    as = asForTable(conn, splitTableName, thisTrack);
    if (! as)
	apiErrAbort(err500, err500Msg, "can not find schema definition for table '%s', track=%s genome: '%s' for endpoint '/list/schema'", splitTableName, track, db);
    columnEl = as->columnList;
    asColumnCount = slCount(columnEl);

    /* do not show counts for protected data */
    if (! protectedTrack(db, thisTrack, track))
	{
	char query[2048];
	sqlSafef(query, sizeof(query), "select count(*) from %s", splitTableName);
	if (hti && hti->isSplit)	/* punting on split table item count */
	    itemCount = 0;
	else
	    {
	    itemCount = sqlQuickNum(conn, query);
	    }
	}
hFreeConn(&conn);


if (hti && (hti->isSplit || debug))
    jsonWriteBoolean(jw, "splitTable", hti->isSplit);

outputSchema(thisTrack, jw, columnNames, columnTypes, jsonTypes, hti,
  columnCount, asColumnCount, columnEl);
    }

outputTrackDbVars(jw, db, thisTrack, itemCount);

apiFinishOutput(0, NULL, jw);

}	/*	static void schemaJsonOutput(FILE *f, char *db, char *track) */

/* typical rsync return
columns: 0              1        2      3         4
drwxrwxr-x            162 2022/10/18 16:58:16 .
drwxrwxr-x          4,096 2023/03/27 16:01:41 bigZips
-r--rw-r--          3,455 2022/08/11 03:26:26 bigZips/GCA_009914755.4_assembly_report.txt
-rw-rw-r--              0 2022/07/18 12:06:00 bigZips/THIS_IS_GENOME_ASSEMBLY_T2T-CHM13v2.0
-rw-rw-r--    812,327,608 2022/07/16 14:27:39 bigZips/hs1.2bit

appears to be a consistent set of columns
*/

/* might be variable depending upon which server request is coming from */
#define DOWNLOAD_HOST "hgdownload.soe.ucsc.edu"

static long long rsyncList(struct jsonWrite *jw, char *db, char *downPath, long long *itemsDone, boolean textOut)
/* rsync listing from hgdownload on the given downPath/db
 *   returning total bytes in the files listing
 */
{
long long totalBytes = 0;
if (*itemsDone >= maxItemsOutput)
    return totalBytes;
boolean reachedMaxItems = FALSE;
int index = 3;	/* rsyncCmd[3] == starts out at NULL, will become the
                 *    hgdownload path */
char *rsyncCmd[] = {"/usr/bin/rsync", "-a", "--list-only", NULL, NULL};
/* rsyncCmd[4] will remain NULL to terminate the list */

struct dyString *tmpDy = dyStringNew(128);
dyStringPrintf(tmpDy, "%s::%s/%s/", DOWNLOAD_HOST, downPath, db);
rsyncCmd[index++] = dyStringCannibalize(&tmpDy);

struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    struct pipeline *dataPipe = pipelineOpen1(rsyncCmd,
       pipelineRead, "/dev/null", NULL, 0);
    FILE *readingLines = pipelineFile(dataPipe);
    char lineBuf[PATH_MAX + 1024];
    while (! reachedMaxItems && fgets(lineBuf, sizeof(lineBuf), readingLines) != NULL)
        {
        if (startsWith("d", lineBuf))
            continue;
        *itemsDone += 1;
        if (*itemsDone > maxItemsOutput)
            {
            reachedMaxItems = TRUE;
            }
        else
            {
            char *columns[5];
            (void) chopByWhite(lineBuf, columns, ArraySize(columns));
            stripChar(columns[1], ',');
            long long bytes = sqlLongLong(columns[1]);
            totalBytes += bytes;
            char outString[PATH_MAX + 1024];
            if (textOut)
                {
                safef(outString, sizeof(outString), "https://%s/%s/%s/%s",
                    DOWNLOAD_HOST, downPath, db, columns[4]);
                textLineOut(outString);
                }
            else
                {
                jsonWriteObjectStart(jw, NULL);
                jsonWriteNumber(jw, "sizeBytes", sqlLongLong(columns[1]));
               safef(outString, sizeof(outString), "%sT%s", columns[2], columns[3]);
                jsonWriteString(jw, "dateTime", outString);
          safef(outString, sizeof(outString), "%s/%s/%s", downPath, db, columns[4]);
                jsonWriteString(jw, "url", outString);
                jsonWriteObjectEnd(jw);
                }
            }
        }
    pipelineClose(&dataPipe);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    apiErrAbort(err400, err400Msg, "can not find genome='%s' for endpoint '/list/files'", db);
    }
errCatchFree(&errCatch);
return totalBytes;
}

static void filesJsonOutput(FILE *f, char *genome, char *fileType, boolean textOut)
/* for given genome, output the URLs to files available on hgdownload
 *   can be a UCSC database genome, or a GenArk hub genome name
 */
{
long long itemsReturned = 0;
boolean genArkHub = FALSE;
char genArkUrl[PATH_MAX + 1024];

if ( isGenArk(genome) )
    {
    genArkHub = TRUE;
    safef(genArkUrl, sizeof(genArkUrl), "hubs/%s", genArkPath(genome));
    }

/* if UCSC genome database, it has already been proven to exist */

struct jsonWrite *jw = NULL;
boolean doContext = (cgiOptionalInt(argSkipContext, 0)==0);
if (textOut && doContext)
    {
    char outString[1024];
    safef(outString, sizeof(outString), "# genome: %s", genome);
    textLineOut(outString);
  safef(outString, sizeof(outString), "# rsyncHost: rsync://%s", DOWNLOAD_HOST);
    textLineOut(outString);
    }
else
    {
    jw = apiStartOutput();
    if (doContext)
        {
        jsonWriteString(jw, "genome", genome);
        jsonWriteString(jw, "rsyncHost", "rsync://" DOWNLOAD_HOST);
        }
    jsonWriteListStart(jw, "urlList");
    }

long long totalBytes = 0;
if (fileType)
    {
    char outString[1024];
    if (genArkHub)
        {
        safef(outString, sizeof(outString), "https://%s/%s/%s/%s.2bit", DOWNLOAD_HOST, genArkUrl, genome, genome);
        textLineOut(outString);
        }
    else
        {
        safef(outString, sizeof(outString), "https://%s/gbdb/%s/%s.2bit", DOWNLOAD_HOST, genome, genome);
        textLineOut(outString);
        }
            
    }
else if (genArkHub)
    {
    totalBytes = rsyncList(jw, genome, genArkUrl, &itemsReturned, textOut);
    }
else
    {
    totalBytes = rsyncList(jw, genome, "goldenPath", &itemsReturned, textOut);
    if (itemsReturned < maxItemsOutput)
       totalBytes += rsyncList(jw, genome, "gbdb", &itemsReturned, textOut);
    if (itemsReturned < maxItemsOutput)
       totalBytes += rsyncList(jw, genome, "mysql", &itemsReturned, textOut);
    }

if (textOut)
    {
    char outString[1024];
    if (doContext)
        {
        safef(outString, sizeof(outString), "# totalBytes: %lld", totalBytes);
        textLineOut(outString);
        }
    if ((itemsReturned > maxItemsOutput) && doContext)
	{
        safef(outString, sizeof(outString), "# maxItemLimit: TRUE");
        textLineOut(outString);
        safef(outString, sizeof(outString), "# itemsReturned: %d", maxItemsOutput);
        textLineOut(outString);
	}
    else
	{
        if (doContext)
            {
       safef(outString, sizeof(outString), "# itemsReturned: %lld", itemsReturned);
            textLineOut(outString);
            }
	}
    textFinishOutput();
    }
else
    {
    jsonWriteListEnd(jw);
    if (doContext)
        {
        jsonWriteNumber(jw, "totalBytes", totalBytes);
        if (itemsReturned > maxItemsOutput)
            {
            jsonWriteBoolean(jw, "maxItemsLimit", TRUE);
            jsonWriteNumber(jw, "itemsReturned", maxItemsOutput);
            }
        else
            jsonWriteNumber(jw, "itemsReturned", itemsReturned);
        }
    apiFinishOutput(0, NULL, jw);
    }
}

static void chromInfoJsonOutput(FILE *f, char *db)
/* for given db, if there is a track, list the chromosomes in that track,
 * for no track, simply list the chromosomes in the sequence
 */
{
char *splitSqlTable = NULL;
struct hTableInfo *tableInfo = NULL;
char *chromName = NULL;
char *table = cgiOptionalString("track");
char *bigDataUrl = NULL;
struct trackDb *thisTrack = NULL;
struct sqlConnection *conn = hAllocConnMaybe(db);
if (NULL == conn)
    apiErrAbort(err400, err400Msg, "can not find 'genome=%s' for endpoint '/list/chromosomes", db);

if (table)
    chromName = validChromName(conn, db, table, &splitSqlTable, &tableInfo);

/* given track can't find a chromName, maybe it is a bigDataUrl */
if (table && ! chromName)
    {
    /* 'track' name in trackDb usually refers to a SQL 'table' */
    struct trackDb *tdb = obtainTdb(NULL, db);
    thisTrack = findTrackDb(table,tdb);
    /* might have a bigDataUrl */
    bigDataUrl = trackDbSetting(thisTrack, "bigDataUrl");
    if (isEmpty(bigDataUrl))
        bigDataUrl = bigDataUrlFromTable(conn, table);
    }

/* in trackDb language: track == table */
/* punting on split tables, just return chromInfo */
if (table && chromName && ! (tableInfo && tableInfo->isSplit) )
    {
    if (! sqlTableExists(conn, splitSqlTable))
	apiErrAbort(err400, err400Msg, "can not find specified 'track=%s' for endpoint: /list/chromosomes?genome=%s;track=%s", table, db, table);

    if (sqlColumnExists(conn, splitSqlTable, chromName))
	{
	char *dataTime = sqlTableUpdate(conn, splitSqlTable);
	time_t dataTimeStamp = sqlDateToUnixTime(dataTime);
	replaceChar(dataTime, ' ', 'T');	/* ISO 8601 */
        struct jsonWrite *jw = apiStartOutput();
	jsonWriteString(jw, "genome", db);
	jsonWriteString(jw, "track", table);
	jsonWriteString(jw, "dataTime", dataTime);
	jsonWriteNumber(jw, "dataTimeStamp", (long long)dataTimeStamp);
	freeMem(dataTime);
        struct slPair *list = NULL;
	char query[2048];
        sqlSafef(query, sizeof(query), "select distinct %s from %s", chromName, splitSqlTable);
	struct sqlResult *sr = sqlGetResult(conn, query);
	char **row;
	while ((row = sqlNextRow(sr)) != NULL)
    	{
            int size = hChromSize(db, row[0]);
	    slAddHead(&list, slPairNew(row[0], intToPt(size)));
    	}
	sqlFreeResult(&sr);
        slPairIntSort(&list);
        slReverse(&list);
        jsonWriteNumber(jw, "chromCount", (long long)slCount(list));
	jsonWriteObjectStart(jw, "chromosomes");
        struct slPair *el = list;
        for ( ; el != NULL; el = el->next )
            jsonWriteNumber(jw, el->name, (long long)ptToInt(el->val));
	jsonWriteObjectEnd(jw);	/* chromosomes */
	apiFinishOutput(0, NULL, jw);
	}
    else
	apiErrAbort(err400, err400Msg, "track '%s' is not a position track, request table without chrom specification, genome: '%s'", table, db);
    }
else if (bigDataUrl)
    {
    struct jsonWrite *jw = apiStartOutput();
    jsonWriteString(jw, "genome", db);
    jsonWriteString(jw, "track", table);
    jsonWriteString(jw, "bigDataUrl", bigDataUrl);
    bigFileChromInfoOutput(jw, thisTrack, bigDataUrl);
    apiFinishOutput(0, NULL, jw);
    }
else if (table && !chromName)	/* only allowing position tables at this time */
	apiErrAbort(err400, err400Msg, "track '%s' is not a position track, request table without chrom specification, genome: '%s'", table, db);
else
    {
    char *dataTime = sqlTableUpdate(conn, "chromInfo");
    time_t dataTimeStamp = sqlDateToUnixTime(dataTime);
    replaceChar(dataTime, ' ', 'T');	/* ISO 8601 */
    struct chromInfo *ciList = createChromInfoList(NULL, db);
    slSort(ciList, chromInfoCmp);
    struct chromInfo *el = ciList;
    struct jsonWrite *jw = apiStartOutput();
    jsonWriteString(jw, "genome", db);
    jsonWriteString(jw, "dataTime", dataTime);
    if (tableInfo && tableInfo->isSplit)	/* the split table punt */
	jsonWriteString(jw, "track", table);
    jsonWriteNumber(jw, "dataTimeStamp", (long long)dataTimeStamp);
    freeMem(dataTime);
    jsonWriteNumber(jw, "chromCount", (long long)slCount(ciList));
    jsonWriteObjectStart(jw, "chromosomes");
    for ( ; el != NULL; el = el->next )
	{
        jsonWriteNumber(jw, el->chrom, (long long)el->size);
	}
    jsonWriteObjectEnd(jw);	/* chromosomes */
    apiFinishOutput(0, NULL, jw);
    }
hFreeConn(&conn);
}

#ifdef NOTUSED
static long long bbiTableItemCount(struct sqlConnection *conn, char *type, char *tableName)
/* Given a tableName that has a fileName column pointing to big*, bam or vcfTabix files, return the
 * total itemCount from all rows (BAM and VCF tables may have one row per chrom). */
{
long long itemCount = 0;
char query[2048];
sqlSafef(query, sizeof query, "select fileName from %s", tableName);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    itemCount += bbiItemCount(hReplaceGbdb(row[0]), type, NULL);
    }
sqlFreeResult(&sr);
return itemCount;
}

static long long dataItemCount(char *db, struct trackDb *tdb)
/* determine how many items are in this data set */
{
long long itemCount = 0;
if (trackHasNoData(tdb))	/* container 'tracks' have no data items */
    return itemCount;
if (protectedTrack(db, tdb, tdb->track))	/*	private data */
    return itemCount;
if (sameWord("downloadsOnly", tdb->type))
    return itemCount;

char *bigDataUrl = hReplaceGbdb(trackDbSetting(tdb, "bigDataUrl"));
if (isNotEmpty(bigDataUrl))
    {
    char *indexFileOrUrl = hReplaceGbdb(trackDbSetting(tdb, "bigDataIndex"));
    itemCount = bbiItemCount(bigDataUrl, tdb->type, indexFileOrUrl);
    }
else
    {
    /* prepare for getting table row count, find table name */
    /* the trackDb might have a specific table defined */
    char *tableName = trackDbSetting(tdb, "table");
    if (isEmpty(tableName))
	tableName = trackDbSetting(tdb, "track");
    if (isNotEmpty(tableName))
	{
	struct sqlConnection *conn = hAllocConnMaybe(db);
	if (conn)
	    {
            if ((startsWith("big", tdb->type) ||
                 sameString("vcfTabix", tdb->type) || sameString("bam", tdb->type)) &&
                sqlColumnExists(conn, tableName, "fileName"))
                {
                itemCount = bbiTableItemCount(conn, tdb->type, tableName);
                }
            else
                {
                /* punting on split tables, return zero */
                struct hTableInfo *hti =
                    hFindTableInfoWithConn(conn, NULL, tableName);
                if (!hti || hti->isSplit)
                    {
                    itemCount = 0;
                    }
                else
                    {
                    char query[2048];
                    sqlSafef(query, sizeof(query), "select count(*) from %s", tableName);
                    itemCount = sqlQuickNum(conn, query);
                    }
                }
            hFreeConn(&conn);
	    }
	}
    }
return itemCount;
}	/*	static long long dataItemCount(char *db, struct trackDb *tdb) */
#endif

static void recursiveTrackList(struct jsonWrite *jw, struct trackDb *tdb,
    char *db)
/* output trackDb tags only for real tracks, not containers,
 * recursive when subtracks exist
 */
{
boolean isContainer = trackHasNoData(tdb);

/* do *NOT* print containers when 'trackLeavesOnly' requested */
if (! (trackLeavesOnly && isContainer) )
    {
#ifdef NOTNOW
    long long itemCount = 0;
    /* do not show counts for protected data or continers (== no items)*/
    if (! (isContainer || protectedTrack(db, tdb, tdb->track)))
	itemCount = dataItemCount(db, tdb);
#endif
    jsonWriteObjectStart(jw, tdb->track);
    if (tdbIsComposite(tdb))
        jsonWriteString(jw, "compositeContainer", "TRUE");
    if (tdbIsCompositeView(tdb))
        jsonWriteString(jw, "compositeViewContainer", "TRUE");
    outputTrackDbVars(jw, db, tdb, -1);

    if (tdb->subtracks)
	{
	struct trackDb *el = NULL;
	for (el = tdb->subtracks; el != NULL; el = el->next )
	    recursiveTrackList(jw, el, db);
	}

    jsonWriteObjectEnd(jw);
    }
else if (tdb->subtracks)
    {
    struct trackDb *el = NULL;
    for (el = tdb->subtracks; el != NULL; el = el->next )
	recursiveTrackList(jw, el, db);
    }
}	/*	static void recursiveTrackList()	*/

static void trackDbJsonOutput(char *db, FILE *f)
/* return track list from specified UCSC database name */
{
struct sqlConnection *conn = hAllocConnMaybe(db);
if (NULL == conn)
    apiErrAbort(err400, err400Msg, "can not find 'genome=%s' for endpoint '/list/tracks", db);

char *dataTime = sqlTableUpdate(conn, "trackDb");
time_t dataTimeStamp = sqlDateToUnixTime(dataTime);
replaceChar(dataTime, ' ', 'T');	/* ISO 8601 */
hFreeConn(&conn);
struct trackDb *tdbList = obtainTdb(NULL, db);
struct jsonWrite *jw = apiStartOutput();
jsonWriteString(jw, "dataTime", dataTime);
jsonWriteNumber(jw, "dataTimeStamp", (long long)dataTimeStamp);
jsonWriteObjectStart(jw, db);
freeMem(dataTime);
struct trackDb *el = NULL;
for (el = tdbList; el != NULL; el = el->next )
    {
    recursiveTrackList(jw, el, db);
    }
jsonWriteObjectEnd(jw);
apiFinishOutput(0, NULL, jw);
}	/*	static void trackDbJsonOutput(char *db, FILE *f)	*/

int trackHubGenomeNameCmp(const void *va, const void *vb)
/* Compare two slNames. */
{
const struct trackHubGenome *a = *((struct trackHubGenome **)va);
const struct trackHubGenome *b = *((struct trackHubGenome **)vb);

return strcmp(a->name, b->name);
}

void trackHubGenomeNameSort(struct trackHubGenome **pList)
/* Sort slName list. */
{
slSort(pList, trackHubGenomeNameCmp);
}

void apiList(char *words[MAX_PATH_INFO])
/* 'list' function words[1] is the subCommand */
{
if (sameWord("publicHubs", words[1]))
    jsonPublicHubs();
else if (sameWord("ucscGenomes", words[1]))
    jsonDbDb();
else if (sameWord("genarkGenomes", words[1]))
    jsonGenArk();
else if (sameWord("hubGenomes", words[1]))
    {
    char *extraArgs = verifyLegalArgs(argListHubGenomes); /* only one allowed */
    if (extraArgs)
	apiErrAbort(err400, err400Msg, "extraneous arguments found for function /list/hubGenomes '%s'", extraArgs);

    char *hubUrl = cgiOptionalString("hubUrl");
    if (isEmpty(hubUrl))
	apiErrAbort(err400, err400Msg, "must supply hubUrl='http:...' some URL to a hub for /list/hubGenomes");

    struct trackHub *hub = errCatchTrackHubOpen(hubUrl);
    if (hub->genomeList)
	{
	trackHubGenomeNameSort(&hub->genomeList);
        struct jsonWrite *jw = apiStartOutput();
	jsonWriteString(jw, "hubUrl", hubUrl);
        jsonWriteObjectStart(jw, "genomes");
	struct trackHubGenome *el;
        for ( el = hub->genomeList; el; el = el->next)
	    {
	    jsonWriteObjectStart(jw, el->name);
	    jsonWriteString(jw, "organism", el->organism);
	    jsonWriteString(jw, "description", el->description);
	    jsonWriteString(jw, "trackDbFile", el->trackDbFile);
	    jsonWriteString(jw, "twoBitPath", el->twoBitPath);
	    jsonWriteString(jw, "groups", el->groups);
	    jsonWriteString(jw, "defaultPos", el->defaultPos);
	    jsonWriteNumber(jw, "orderKey", el->orderKey);
	    jsonWriteObjectEnd(jw);
	    }
	jsonWriteObjectEnd(jw);
	apiFinishOutput(0, NULL, jw);
	}
    }
else if (sameWord("tracks", words[1]))
    {
    char *extraArgs = verifyLegalArgs(argListTracks);
    if (extraArgs)
	apiErrAbort(err400, err400Msg, "extraneous arguments found for function /list/tracks '%s'", extraArgs);

    char *hubUrl = cgiOptionalString("hubUrl");
    char *genome = cgiOptionalString("genome");
    char *db = cgiOptionalString("genome");
    /* allow a GCx genome specified without hubUrl for GenArk genomes */
    if (isEmpty(hubUrl) && isNotEmpty(genome))
	hubUrl = genarkUrl(genome);

    if (isEmpty(hubUrl) && isNotEmpty(db))
	{
	struct sqlConnection *conn = hAllocConnMaybe(db);
        if (NULL == conn)
	    apiErrAbort(err400, err400Msg, "can not find 'genome=%s' for endpoint '/list/tracks", db);
	else
	    hFreeConn(&conn);
	}
    if (isEmpty(hubUrl) && isEmpty(db))
      apiErrAbort(err400, err400Msg, "missing hubUrl or genome name for endpoint /list/tracks");
    if (isEmpty(hubUrl))	// missing hubUrl implies UCSC database
	{
        trackDbJsonOutput(db, stdout);	// only need db for this function
	return;
	}
    if (isEmpty(genome) || isEmpty(hubUrl))
	{
        if (isEmpty(genome))
	    apiErrAbort(err400, err400Msg, "must supply genome='someName' the name of a genome in a hub for /list/tracks\n");
	if (isEmpty(hubUrl))
            apiErrAbort(err400, err400Msg, "must supply hubUrl='http:...' some URL to a hub for /list/tracks");
	}
    struct trackHub *hub = errCatchTrackHubOpen(hubUrl);
    struct trackHubGenome *hubGenome = findHubGenome(hub, genome,
	"/list/tracks", hubUrl);
    struct trackDb *tdbList = obtainTdb(hubGenome, NULL);
    struct jsonWrite *jw = apiStartOutput();
    jsonWriteString(jw, "hubUrl", hubUrl);
    jsonWriteObjectStart(jw, hubGenome->name);
    struct trackDb *el = NULL;
    for (el = tdbList; el != NULL; el = el->next )
	    {
	    recursiveTrackList(jw, el, db);
	    }
    jsonWriteObjectEnd(jw);
    apiFinishOutput(0, NULL, jw);
    }
else if (sameWord("chromosomes", words[1]))
    {
    char *extraArgs = verifyLegalArgs(argListChromosomes);
    if (extraArgs)
	apiErrAbort(err400, err400Msg, "extraneous arguments found for function /list/chromosomes '%s'", extraArgs);

    char *hubUrl = cgiOptionalString("hubUrl");
    char *genome = cgiOptionalString("genome");
    char *db = cgiOptionalString("genome");
    /* allow a GCx genome specified without hubUrl for GenArk genomes */
    if (isEmpty(hubUrl) && isNotEmpty(genome))
	hubUrl = genarkUrl(genome);

    if (isEmpty(hubUrl) && isNotEmpty(db))
	{
	struct sqlConnection *conn = hAllocConnMaybe(db);
        if (NULL == conn)
	    apiErrAbort(err400, err400Msg, "can not find 'genome=%s' for endpoint '/list/chromosomes", db);
	else
	    hFreeConn(&conn);
	}
    if (isEmpty(hubUrl) && isEmpty(db))
        apiErrAbort(err400, err400Msg, "must supply hubUrl or genome name for endpoint '/list/chromosomes", hubUrl, db);

    if (isEmpty(hubUrl))	// missing hubUrl implies UCSC database
	{
        chromInfoJsonOutput(stdout, db);
	return;
	}
    else
	{
        hubChromInfoJsonOutput(stdout, hubUrl, genome);
	return;
	}
    }
else if (sameWord("schema", words[1]))
    {
    char *extraArgs = verifyLegalArgs(argListSchema);
    if (extraArgs)
	apiErrAbort(err400, err400Msg, "extraneous arguments found for function /list/schema '%s'", extraArgs);

    char *hubUrl = cgiOptionalString("hubUrl");
    char *genome = cgiOptionalString("genome");
    char *db = cgiOptionalString("genome");
    char *track = cgiOptionalString("track");
    /* allow a GCx genome specified without hubUrl for GenArk genomes */
    if (isEmpty(hubUrl) && isNotEmpty(genome))
	hubUrl = genarkUrl(genome);

    if (isEmpty(track))
	apiErrAbort(err400, err400Msg, "missing track=<name> for endpoint '/list/schema'");

    if (isEmpty(hubUrl) && isNotEmpty(db))
	{
	struct sqlConnection *conn = hAllocConnMaybe(db);
        if (NULL == conn)
	    apiErrAbort(err400, err400Msg, "can not find 'genome=%s' for endpoint '/list/schema", db);
	else
	    hFreeConn(&conn);
	}

    if (isEmpty(hubUrl) && isEmpty(db))
        apiErrAbort(err400, err400Msg, "must supply hubUrl or genome name for endpoint '/list/schema", hubUrl, db);

    if (isEmpty(hubUrl))	// missing hubUrl implies UCSC database
	{
        schemaJsonOutput(stdout, db, track);
	return;
	}
    else
	{
        hubSchemaJsonOutput(stdout, hubUrl, genome, track);
	return;
	}
    }
else if (sameWord("files", words[1]))
    {
    boolean textOut = FALSE;
    char *extraArgs = verifyLegalArgs(argListFiles);
    if (extraArgs)
	apiErrAbort(err400, err400Msg, "extraneous arguments found for function /list/files '%s', only 'genome', 'fileType', 'skipContext' and 'format' are allowed.", extraArgs);

    char *genome = cgiOptionalString("genome");
    char *format = cgiOptionalString("format");
    char *fileType = cgiOptionalString("fileType");
    if (isEmpty(genome))
        apiErrAbort(err400, err400Msg, "must supply a genome name for endpoint '/list/files' (a database name or GenArk genome name, e.g.: 'hg38' or 'GCA_021951015.1'");
    if (isNotEmpty(format))
	{
	if (sameWord("text", format))
	    textOut = TRUE;
        else
	    apiErrAbort(err400, err400Msg, "only format=text allowed for endpoint '/list/files', found: format=%s", format);
	}
    filesJsonOutput(stdout, genome, fileType, textOut);
    }
else
    apiErrAbort(err400, err400Msg, "do not recognize endpoint function: '/%s/%s'", words[0], words[1]);
}	/*	void apiList(char *words[MAX_PATH_INFO])        */
