/* hdb - human genome browser database. */
#include "common.h"
#include "portable.h"
#include "hash.h"
#include "binRange.h"
#include "jksql.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nib.h"
#include "hdb.h"
#include "hgRelate.h"
#include "fa.h"
#include "hgConfig.h"
#include "ctgPos.h"
#include "trackDb.h"
#include "hCommon.h"
#include "hgFind.h"
#include "dbDb.h"
#include "axtInfo.h"
#include "subText.h"
#include "blatServers.h"
#include "bed.h"

static struct sqlConnCache *hdbCc = NULL;  /* cache for primary database connection */
static struct sqlConnCache *hdbCc2 = NULL;  /* cache for second database connection (ortholog) */
static struct sqlConnCache *centralCc = NULL;

#define DEFAULT_HUMAN "hg12"
#define DEFAULT_MOUSE "mm2"
#define DEFAULT_ZOO   "zooHuman3"
#define DEFAULT_RAT   "rn1"

static char *defaultHuman = DEFAULT_HUMAN;
static char *defaultMouse = DEFAULT_MOUSE;
static char *defaultZoo   = DEFAULT_ZOO;
static char *defaultRat   = DEFAULT_RAT;

static char *hdbHost;
static char *hdbName = DEFAULT_HUMAN;
static char *hdbName2 = DEFAULT_MOUSE;
static char *hdbUser;
static char *hdbPassword;
static char *hdbTrackDb = NULL;

static char* getCfgValue(char* envName, char* cfgName)
/* get a configuration value, from either the environment or the cfg file,
 * with the env take precedence.
 */
{
char *val = getenv(envName);
if (val == NULL)
    val = cfgOption(cfgName);
return val;
}

void hDefaultConnect()
/* read in the connection options from config file */
{
hdbHost 	= getCfgValue("HGDB_HOST", "db.host");
hdbUser 	= getCfgValue("HGDB_USER", "db.user");
hdbPassword	= getCfgValue("HGDB_PASSWORD", "db.password");
hdbTrackDb      = getCfgValue("HGDB_TRACKDB", "db.trackDb");
if(hdbHost == NULL || hdbUser == NULL || hdbPassword == NULL)
    errAbort("cannot read in connection setting from configuration file.");
}

char *hTrackDbName()
/* return the name of the track database from the config file. Freez when done */
{
if(hdbTrackDb == NULL)
    hdbTrackDb = cfgOption("db.trackDb");
if(hdbTrackDb == NULL)
    errAbort("Please set the db.trackDb field in the hg.conf config file.");
return cloneString(hdbTrackDb);
}

void hSetDbConnect(char* host, char *db, char *user, char *password)
/* set the connection information for the database */
{
    hdbHost = host;
    hdbName = db;
    hdbUser = user;
    hdbPassword = password;
}
void hSetDbConnect2(char* host, char *db, char *user, char *password)
/* set the connection information for the database */
{
    hdbHost = host;
    hdbName2 = db;
    hdbUser = user;
    hdbPassword = password;
}

void hSetDb(char *dbName)
/* Set the database name. */
{
if (hdbCc != NULL)
    errAbort("Can't hgSetDb after an hgAllocConn(), sorry.");
hdbName = dbName;
}

void hSetDb2(char *dbName)
/* Set the database name. */
{
if (hdbCc2 != NULL)
    errAbort("Can't hgSetDb after an hgAllocConn2(), sorry.");
hdbName2 = dbName;
}

char *hGetDb()
/* Return the current database name. */
{
return hdbName;
}

char *hGetDb2()
/* Return the current database name. */
{
return hdbName2;
}

char *hGetDbHost()
/* Return the current database host. */
{
return hdbHost;
}

char *hGetDbName()
/* Return the current database name. */
{
return hdbName;
}

char *hGetDbUser()
/* Return the current database user. */
{
return hdbUser;
}

char *hGetDbPassword()
/* Return the current database password. */
{
return hdbPassword;
}

struct sqlConnection *hAllocConn()
/* Get free connection if possible. If not allocate a new one. */
{
if (hdbHost == NULL)
    hDefaultConnect();
if (hdbCc == NULL)
    hdbCc = sqlNewRemoteConnCache(hdbName, hdbHost, hdbUser, hdbPassword);
return sqlAllocConnection(hdbCc);
}

struct sqlConnection *hAllocConn2()
/* Get free connection if possible. If not allocate a new one. */
{
if (hdbHost == NULL)
    hDefaultConnect();
if (hdbCc2 == NULL)
    hdbCc2 = sqlNewRemoteConnCache(hdbName2, hdbHost, hdbUser, hdbPassword);
return sqlAllocConnection(hdbCc2);
}

void hFreeConn(struct sqlConnection **pConn)
/* Put back connection for reuse. */
{
sqlFreeConnection(hdbCc, pConn);
}

void hFreeConn2(struct sqlConnection **pConn)
/* Put back connection for reuse into second pool for second database connection */
{
sqlFreeConnection(hdbCc2, pConn);
}

struct sqlConnection *hConnectCentral()
/* Connect to central database where user info and other info
 * not specific to a particular genome lives.  Free this up
 * with hDisconnectCentral(). */
{
if (centralCc == NULL)
    {
    char *database = cfgOption("central.db");
    char *host = cfgOption("central.host");
    char *user = cfgOption("central.user");
    char *password = cfgOption("central.password");;

    if (host == NULL || user == NULL || password == NULL)
	errAbort("Please set central options in the hg.conf file.");
    centralCc = sqlNewRemoteConnCache(database, host, user, password);
    }
return sqlAllocConnection(centralCc);
}

void hDisconnectCentral(struct sqlConnection **pConn)
/* Put back connection for reuse. */
{
sqlFreeConnection(centralCc, pConn);
}

boolean hTableExists(char *table)
/* Return TRUE if a table exists in database. */
{
struct sqlConnection *conn = hAllocConn();
boolean exists = sqlTableExists(conn, table);
hFreeConn(&conn);
return exists;
}

boolean hTableExists2(char *table)
/* Return TRUE if a table exists in secondary database. */
{
struct sqlConnection *conn = hAllocConn2();
boolean exists = sqlTableExists(conn, table);
hFreeConn2(&conn);
return exists;
}


void hParseTableName(char *table, char trackName[128], char chrom[32])
/* Parse an actual table name like "chr17_random_blastzWhatever" into 
 * the track name (blastzWhatever) and chrom (chr17_random). */
{
char *ptr;

/* It might not be a split table; provide defaults: */
strncpy(trackName, table, 128);
strcpy(chrom, "chr1");
if (startsWith("chr", table))
    {
    if ((ptr = strstr(table, "_random_")) != NULL)
	{
	strncpy(trackName, ptr+strlen("_random_"), 128);
	strncpy(chrom, table, 32);
	if ((ptr = strstr(chrom, "_random")) != NULL)
	    *(ptr+strlen("_random")) = 0;
	}
    else if ((ptr = strchr(table, '_')) != NULL)
	{
	strncpy(trackName, ptr+1, 128);
	strncpy(chrom, table, 32);
	if ((ptr = strchr(chrom, '_')) != NULL)
	    *ptr = 0;
	}
    }
}


int hChromSize(char *chromName)
/* Return size of chromosome. */
{
struct sqlConnection *conn;
int size;
char query[256];

conn = hAllocConn();
sprintf(query, "select size from chromInfo where chrom = '%s'", chromName);
size = sqlQuickNum(conn, query);
hFreeConn(&conn);
return size;
}

int hChromSize2(char *chromName)
/* Return size of chromosome. */
{
struct sqlConnection *conn;
int size;
char query[256];

conn = hAllocConn2();
sprintf(query, "select size from chromInfo where chrom = '%s'", chromName);
size = sqlQuickNum(conn, query);
hFreeConn2(&conn);
return size;
}

void hNibForChrom(char *chromName, char retNibName[512])
/* Get .nib file associated with chromosome. */
{
struct sqlConnection *conn;
char query[256];
conn = hAllocConn();
sprintf(query, "select fileName from chromInfo where chrom = '%s'", chromName);
if (sqlQuickQuery(conn, query, retNibName, 512) == NULL)
    errAbort("Sequence %s isn't in database", chromName);
hFreeConn(&conn);
}

struct hash *hCtgPosHash()
/* Return hash of ctgPos from current database keyed by contig name. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hash *hash = newHash(10);
struct ctgPos *ctg;

conn = hAllocConn();
sr = sqlGetResult(conn, "select * from ctgPos");
while ((row = sqlNextRow(sr)) != NULL)
    {
    ctg = ctgPosLoad(row);
    hashAdd(hash, ctg->contig, ctg);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return hash;
}

struct dnaSeq *hChromSeq(char *chrom, int start, int end)
/* Return lower case DNA from chromosome. */
{
char fileName[512];
hNibForChrom(chrom, fileName);
return nibLoadPart(fileName, start, end-start);
}

boolean hChromBandConn(struct sqlConnection *conn, 
	char *chrom, int pos, char retBand[64])
/* Return text string that says what band pos is on. 
 * Return FALSE if not on any band, or table missing. */
{
char query[256];
char buf[64];
char *s;
boolean ok = TRUE;

sprintf(query, 
	"select name from cytoBand where chrom = '%s' and chromStart <= %d and chromEnd > %d", 
	chrom, pos, pos);
buf[0] = 0;
s = sqlQuickQuery(conn, query, buf, sizeof(buf));
if (s == NULL)
   {
   s = "";
   ok = FALSE;
   }
sprintf(retBand, "%s%s", skipChr(chrom), buf);
return ok;
}

boolean hChromBand(char *chrom, int pos, char retBand[64])
/* Return text string that says what band pos is on. 
 * Return FALSE if not on any band, or table missing. */
{
if (!hTableExists("cytoBand"))
    return FALSE;
else
    {
    struct sqlConnection *conn = hAllocConn();
    boolean ok = hChromBandConn(conn, chrom, pos, retBand);
    hFreeConn(&conn);
    return ok;
    }
}


struct dnaSeq *hDnaFromSeq(char *seqName, int start, int end, enum dnaCase dnaCase)
/* Fetch DNA */
{
char fileName[512];
struct dnaSeq *seq = hChromSeq(seqName, start, end);
if (dnaCase == dnaUpper)
    touppers(seq->dna);
return seq;
}

struct dnaSeq *hLoadChrom(char *chromName)
/* Fetch entire chromosome into memory. */
{
int size = hChromSize(chromName);
return hDnaFromSeq(chromName, 0, size, dnaLower);
}

struct slName *hAllChromNames()
/* Get list of all chromosome names. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
struct slName *list = NULL, *el;
char **row;

sr = sqlGetResult(conn, "select chrom from chromInfo");
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = newSlName(row[0]);
    slAddHead(&list, el);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&list);
return list;
}

struct largeSeqFile
/* Manages our large external sequence files.  Typically there will
 * be around four of these.  This basically caches the file handle
 * so don't have to keep opening and closing them. */
{
    struct largeSeqFile *next;  /* Next in list. */
    char *path;                 /* Path name for file. */
    HGID id;                    /* Id in extFile table. */
    int fd;                     /* File handle. */
    };

static struct largeSeqFile *largeFileList;  /* List of open large files. */

struct largeSeqFile *largeFileHandle(HGID extId)
/* Return handle to large external file. */
{
struct largeSeqFile *lsf;

/* Search for it on existing list and return it if found. */
for (lsf = largeFileList; lsf != NULL; lsf = lsf->next)
    {
    if (lsf->id == extId)
        return lsf;
    }

/* Open file and put it on list. */
    {
    struct largeSeqFile *lsf;
    struct sqlConnection *conn = hgAllocConn();
    char query[256];
    struct sqlResult *sr;
    char **row;
    long size;
    char *path;

    /* Query database to find full path name and size file should be. */
    sprintf(query, "select path,size from extFile where id=%u", extId);
    sr = sqlGetResult(conn,query);
    if ((row = sqlNextRow(sr)) == NULL)
        errAbort("Database inconsistency - no external file with id %lu", extId);

    /* Save info on list. Check that file size is what we think it should be. */
    AllocVar(lsf);
    lsf->path = path = cloneString(row[0]);
    size = sqlUnsigned(row[1]);
    if (fileSize(path) != size)
        errAbort("External file %s has changed, need to resync database.  Old size %ld, new size %ld", path, size, fileSize(path));
    lsf->id = extId;
    if ((lsf->fd = open(path, O_RDONLY)) < 0)
        errAbort("Couldn't open external file %s", path);
    slAddHead(&largeFileList, lsf);
    sqlFreeResult(&sr);
    hgFreeConn(&conn);
    return lsf;
    }
}

void *readOpenFileSection(int fd, unsigned long offset, size_t size, char *fileName)
/* Allocate a buffer big enough to hold a section of a file,
 * and read that section into it. */
{
void *buf;
buf = needMem(size+1);
if (lseek(fd, offset, SEEK_SET) < 0)
        errAbort("Couldn't seek to %ld in %s", offset, fileName);
if (read(fd, buf, size) < size)
        errAbort("Couldn't read %u bytes at %ld in %s", size, offset, fileName);
return buf;
}


void hRnaSeqAndId(char *acc, struct dnaSeq **retSeq, HGID *retId)
/* Return sequence for RNA and it's database ID. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];
int fd;
HGID extId;
size_t size;
unsigned long offset;
char *buf;
struct dnaSeq *seq;
struct largeSeqFile *lsf;

sprintf(query,
   "select id,extFile,file_offset,file_size from seq where seq.acc = '%s'",
   acc);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row == NULL)
    errAbort("No sequence for %s in database", acc);
*retId = sqlUnsigned(row[0]);
extId = sqlUnsigned(row[1]);
offset = sqlUnsigned(row[2]);
size = sqlUnsigned(row[3]);
lsf = largeFileHandle(extId);
buf = readOpenFileSection(lsf->fd, offset, size, lsf->path);
*retSeq = seq = faFromMemText(buf);
sqlFreeResult(&sr);
hFreeConn(&conn);
}


struct dnaSeq *hExtSeq(char *acc)
/* Return sequence for external sequence. */
{
struct dnaSeq *seq;
HGID id;
hRnaSeqAndId(acc, &seq, &id);
return seq;
}

struct dnaSeq *hRnaSeq(char *acc)
/* Return sequence for RNA. */
{
return hExtSeq(acc);
}

struct bed *hGetBedRangeDb(char *db, char *table, char *chrom, int chromStart,
			   int chromEnd, char *sqlConstraints)
/* Return a bed list of all items (that match sqlConstraints, if nonNULL) 
   in the given range in table. */
{
struct dyString *query = newDyString(512);
struct sqlConnection *conn;
struct sqlResult *sr;
struct hTableInfo *hti;
struct bed *bedList=NULL, *bedItem;
char **row;
char parsedChrom[32];
char rootName[256];
char fullTableName[256];
char rangeStr[32];
int count;
boolean canDoUTR, canDoIntrons;
boolean useSqlConstraints = sqlConstraints != NULL && sqlConstraints[0] != 0;
char tStrand = '?', qStrand = '?';
int i;

/* Caller can give us either a full table name or root table name. */
hParseTableName(table, rootName, parsedChrom);
hti = hFindTableInfoDb(db, chrom, rootName);
if (hti == NULL)
    errAbort("Could not find table info for table %s (%s)",
	     rootName, table);
if (hti->isSplit)
    snprintf(fullTableName, sizeof(fullTableName), "%s_%s", chrom, rootName);
else
    strncpy(fullTableName, rootName, sizeof(fullTableName));
canDoUTR = hti->hasCDS;
canDoIntrons = hti->hasBlocks;

if (sameString(db, hGetDb()))
    conn = hAllocConn();
else if (sameString(db, hGetDb2()))
    conn = hAllocConn2();
else
    {
    hSetDb2(db);
    conn = hAllocConn2();
    }
dyStringClear(query);
// row[0], row[1] -> start, end
dyStringPrintf(query, "SELECT %s,%s", hti->startField, hti->endField);
// row[2] -> name or placeholder
if (hti->nameField[0] != 0)
    dyStringPrintf(query, ",%s", hti->nameField);
else
    dyStringPrintf(query, ",%s", hti->startField);  // keep the same #fields!
// row[3] -> score or placeholder
if (hti->scoreField[0] != 0)
    dyStringPrintf(query, ",%s", hti->scoreField);
else
    dyStringPrintf(query, ",%s", hti->startField);  // keep the same #fields!
// row[4] -> strand or placeholder
if (hti->strandField[0] != 0)
    dyStringPrintf(query, ",%s", hti->strandField);
else
    dyStringPrintf(query, ",%s", hti->startField);  // keep the same #fields!
// row[5], row[6] -> cdsStart, cdsEnd or placeholders
if (hti->cdsStartField[0] != 0)
    dyStringPrintf(query, ",%s,%s", hti->cdsStartField, hti->cdsEndField);
else
    dyStringPrintf(query, ",%s,%s", hti->startField, hti->startField);  // keep the same #fields!
// row[7], row[8], row[9] -> count, starts, ends/sizes or empty.
if (hti->countField[0] != 0)
    dyStringPrintf(query, ",%s,%s,%s", hti->countField, hti->startsField,
		   hti->endsSizesField);
else
    dyStringPrintf(query, ",%s,%s,%s", hti->startField, hti->startField,
		   hti->startField);  // keep same #fields!
// row[10] -> tSize for PSL '-' strand coord-swizzling only:
if (sameString("tStarts", hti->startsField))
    dyStringAppend(query, ",tSize");
else
    dyStringPrintf(query, ",%s", hti->startField);  // keep the same #fields!
dyStringPrintf(query, " FROM %s WHERE %s < %d AND %s > %d",
	       fullTableName,
	       hti->startField, chromEnd, hti->endField, chromStart);
if (hti->chromField[0] != 0)
    dyStringPrintf(query, " AND %s = '%s'", hti->chromField, chrom);
if (useSqlConstraints)
    dyStringPrintf(query, " AND %s", sqlConstraints);

sr = sqlGetResult(conn, query->string);

while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(bedItem);
    bedItem->chrom      = cloneString(chrom);
    bedItem->chromStart = atoi(row[0]);
    bedItem->chromEnd   = atoi(row[1]);
    if (hti->nameField[0] != 0)
	bedItem->name   = cloneString(row[2]);
    else
	{
	snprintf(rangeStr, sizeof(rangeStr), "%s:%d-%d", chrom,
		 bedItem->chromStart+1,  bedItem->chromEnd);
	bedItem->name   = cloneString(rangeStr);
	}
    if (hti->scoreField[0] != 0)
	bedItem->score  = atoi(row[3]);
    else
	bedItem->score  = 0;
    if (hti->strandField[0] != 0)
	if (sameString("tStarts", hti->startsField))
	    {
	    // psl: use XOR of qStrand,tStrand
	    qStrand = row[4][0];
	    tStrand = row[4][1];
	    if ((qStrand == '-' && tStrand == '+') ||
		(qStrand == '+' && tStrand == '-'))
		strncpy(bedItem->strand, "-", 2);
	    else
		strncpy(bedItem->strand, "+", 2);
	    }
	else
	    strncpy(bedItem->strand, row[4], 2);
    else
	strcpy(bedItem->strand, "?");
    if (canDoUTR)
	{
	bedItem->thickStart = atoi(row[5]);
	bedItem->thickEnd   = atoi(row[6]);
	/* thickStart, thickEnd fields are sometimes used for other-organism 
	   coords (e.g. synteny100000, syntenyBuild30).  So if they look 
	   completely wrong, fake them out to start/end.  */
	if (bedItem->thickStart < bedItem->chromStart)
	    bedItem->thickStart = bedItem->chromStart;
	else if (bedItem->thickStart > bedItem->chromEnd)
	    bedItem->thickStart = bedItem->chromStart;
	if (bedItem->thickEnd < bedItem->chromStart)
	    bedItem->thickEnd = bedItem->chromEnd;
	else if (bedItem->thickEnd > bedItem->chromEnd)
	    bedItem->thickEnd = bedItem->chromEnd;
	}
    else
	{
	bedItem->thickStart = bedItem->chromStart;
	bedItem->thickEnd   = bedItem->chromEnd;
	}
    if (canDoIntrons)
	{
	bedItem->blockCount = atoi(row[7]);
	sqlSignedDynamicArray(row[8], &bedItem->chromStarts, &count);
	assert(count == bedItem->blockCount);
	sqlSignedDynamicArray(row[9], &bedItem->blockSizes, &count);
	assert(count == bedItem->blockCount);
	if (sameString("exonEnds", hti->endsSizesField))
	    {
	    // genePred: translate ends to sizes
	    for (i=0;  i < bedItem->blockCount;  i++)
		{
		bedItem->blockSizes[i] -= bedItem->chromStarts[i];
		}
	    }
	if (sameString("tStarts", hti->startsField) && (tStrand == '-'))
	    {
	    // psl: if target strand is '-', flip the coords.
	    // (this is the target part of pslRcBoth from src/lib/psl.c)
	    int tSize = atoi(row[10]);
	    for (i=0; i<bedItem->blockCount; ++i)
		{
		bedItem->chromStarts[i] = tSize - (bedItem->chromStarts[i] +
						   bedItem->blockSizes[i]);
		}
	    reverseInts(bedItem->chromStarts, bedItem->blockCount);
	    reverseInts(bedItem->blockSizes, bedItem->blockCount);
	    assert(bedItem->chromStart == bedItem->chromStarts[0]);
	    }
	if (! (sameString("chromStarts", hti->startsField) ||
	       sameString("blockStarts", hti->startsField)) )
	    {
	    // non-bed: translate absolute starts to relative starts
	    for (i=0;  i < bedItem->blockCount;  i++)
		{
		bedItem->chromStarts[i] -= bedItem->chromStart;
		}
	    }
	}
    else
	{
	bedItem->blockCount  = 0;
	bedItem->chromStarts = NULL;
	bedItem->blockSizes  = NULL;
	}
    slAddHead(&bedList, bedItem);
    }
dyStringFree(&query);
sqlFreeResult(&sr);
if (sameString(db, hGetDb()))
    hFreeConn(&conn);
else
    hFreeConn2(&conn);
slReverse(&bedList);
return(bedList);
}


struct bed *hGetBedRange(char *table, char *chrom, int chromStart,
			 int chromEnd, char *sqlConstraints)
/* Return a bed list of all items (that match sqlConstraints, if nonNULL) 
   in the given range in table. */
{
return(hGetBedRangeDb(hGetDb(), table, chrom, chromStart, chromEnd,
		      sqlConstraints));
}


static char *hFreezeDbConversion(char *database, char *freeze)
/* Find freeze given database or vice versa.  Pass in NULL
 * for parameter that is unknown and it will be returned
 * as a result.  This result can be freeMem'd when done. */
{
struct sqlConnection *conn = hConnectCentral();
struct sqlResult *sr;
char **row;
char *ret = NULL;
struct dyString *dy = newDyString(128);

if (database != NULL)
    dyStringPrintf(dy, "select description from dbDb where name = '%s'", database);
else if (freeze != NULL)
    dyStringPrintf(dy, "select name from dbDb where description = '%s'", freeze);
else
    internalErr();
sr = sqlGetResult(conn, dy->string);
if ((row = sqlNextRow(sr)) != NULL)
    ret = cloneString(row[0]);
sqlFreeResult(&sr);
hDisconnectCentral(&conn);
freeDyString(&dy);
return ret;
}


char *hFreezeFromDb(char *database)
/* return the freeze for the database version. 
   For example: "hg6" returns "Dec 12, 2000". If database
   not recognized returns NULL */
{
return hFreezeDbConversion(database, NULL);
}

char *hDbFromFreeze(char *freeze)
/* Return database version from freeze name. */
{
return hFreezeDbConversion(NULL, freeze);
}

char *hDefaultPos(char *database)
/* Return default chromosome position for the 
  organism associated with database.   use freeMem on
 * this when done. */
{
struct sqlConnection *conn = hConnectCentral();
char buf[128];
char query[256];
char *res = NULL;
sprintf(query, "select defaultPos from dbDb where name = '%s'", database);
if (sqlQuickQuery(conn, query, buf, sizeof(buf)) != NULL)
    res = cloneString(buf);
else
    errAbort("Can't find default position for %s", database);
hDisconnectCentral(&conn);
return res;
}

char *hOrganism(char *database)
/* Return organism associated with database.   use freeMem on
 * this when done. */
{
struct sqlConnection *conn = hConnectCentral();
char buf[128];
char query[256];
char *res = NULL;
sprintf(query, "select organism from dbDb where name = '%s'", database);
if (sqlQuickQuery(conn, query, buf, sizeof(buf)) != NULL)
    res = cloneString(buf);
else
    errAbort("Can't find organism for %s", database);
hDisconnectCentral(&conn);
return res;
}

char *hLookupStringVars(char *in, char *database)
/* Expand $ORGANISM and other variables in input. */
{
static struct subText *subList = NULL;
static char *oldDatabase = NULL;

if (oldDatabase != NULL && !sameString(database, oldDatabase))
    {
    subTextFreeList(&subList);
    freez(&oldDatabase);
    oldDatabase = cloneString(database);
    }
if (subList == NULL)
    {
    struct subText *sub;
    char *organism = hOrganism(database);
    char *lcOrg = cloneString(organism);
    char *ucOrg = cloneString(organism);
    tolowers(lcOrg);
    touppers(ucOrg);
    sub = subTextNew("$Organism", organism);
    slAddHead(&subList, sub);
    sub = subTextNew("$ORGANISM", ucOrg);
    slAddHead(&subList, sub);
    sub = subTextNew("$organism", lcOrg);
    slAddHead(&subList, sub);
    freez(&ucOrg);
    freez(&lcOrg);
    freez(&organism);
    }
return subTextString(subList, in);
}

static void subOut(char **pString, char *database)
/* Substitute one string. */
{
char *old = *pString;
*pString = hLookupStringVars(old, database);
freeMem(old);
}

void hLookupStringsInTdb(struct trackDb *tdb, char *database)
/* Lookup strings in track database. */
{
subOut(&tdb->shortLabel, database);
subOut(&tdb->longLabel, database);
subOut(&tdb->html, database);
}


struct dbDb *hDbDbList()
/* Return list of databases that are actually online. 
 * The list includes the name, description, and where to
 * find the nib-formatted DNA files. Free this with dbDbFree. */
{
struct sqlConnection *conn = hConnectCentral();
struct sqlResult *sr;
char **row;
struct dbDb *dbList = NULL, *db;
struct hash *hash = sqlHashOfDatabases();

sr = sqlGetResult(conn, "select * from dbDb");
while ((row = sqlNextRow(sr)) != NULL)
    {
    db = dbDbLoad(row);
    if (hashLookup(hash, db->name))
        {
	slAddHead(&dbList, db);
	}
    else
        dbDbFree(&db);
    }
sqlFreeResult(&sr);
hashFree(&hash);
hDisconnectCentral(&conn);
slReverse(&dbList);
return dbList;
}

struct slName *hDbList()
/* List of all database versions that are online (database
 * names only).  See also hDbDbList. */
{
struct slName *nList = NULL, *n;
struct dbDb *dbList, *db;

dbList = hDbDbList();
for (db = dbList; db != NULL; db = db->next)
    {
    n = newSlName(db->name);
    slAddTail(&nList, n);
    }
dbDbFree(&dbList);
return nList;
}


static boolean fitField(struct hash *hash, char *fieldName,
	char retField[32])
/* Return TRUE if fieldName is in hash.  
 * If so copy it to retField.
 * Helper routine for findMoreFields below. */
{
if (hashLookup(hash, fieldName))
    {
    strcpy(retField, fieldName);
    return TRUE;
    }
else
    {
    retField[0] = 0;
    return FALSE;
    }
}

static boolean fitFields(struct hash *hash, char *chrom, char *start, char *end,
	char retChrom[32], char retStart[32], char retEnd[32])
/* Return TRUE if chrom/start/end are in hash.  
 * If so copy them to retChrom, retStart, retEnd. 
 * Helper routine for findChromStartEndFields below. */
{
if (hashLookup(hash, chrom) && hashLookup(hash, start) && hashLookup(hash, end))
    {
    strcpy(retChrom, chrom);
    strcpy(retStart, start);
    strcpy(retEnd, end);
    return TRUE;
    }
else
    return FALSE;
}

boolean hIsBinned(char *table)
/* Return TRUE if a table is binned. */
{
char query[256];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
boolean binned = FALSE;

/* Read table description into hash. */
sprintf(query, "describe %s", table);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    if (sameString(row[0], "bin"))
        binned = TRUE;
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return binned;
}


boolean hFindBed12FieldsAndBinDb(char *db, char *table, 
	char retChrom[32], char retStart[32], char retEnd[32],
	char retName[32], char retScore[32], char retStrand[32],
        char retCdsStart[32], char retCdsEnd[32],
	char retCount[32], char retStarts[32], char retEndsSizes[32],
        boolean *retBinned)
/* Given a table return the fields corresponding to all the bed 12 
 * fields, if they exist.  Fields that don't exist in the given table 
 * will be set to "". */
{
char query[256];
struct sqlConnection *conn;
struct sqlResult *sr;
char **row;
struct hash *hash = newHash(5);
boolean gotIt = TRUE, binned = FALSE;

if (sameString(db, hGetDb()))
    conn = hAllocConn();
else if (sameString(db, hGetDb2()))
    conn = hAllocConn2();
else
    {
    hSetDb2(db);
    conn = hAllocConn2();
    }

if (! sqlTableExists(conn, table))
    {
    if (sameString(db, hGetDb()))
	hFreeConn(&conn);
    else
	hFreeConn2(&conn);
    return FALSE;
    }

/* Read table description into hash. */
sprintf(query, "describe %s", table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (sameString(row[0], "bin"))
        binned = TRUE;
    hashAdd(hash, row[0], NULL);
    }
sqlFreeResult(&sr);

/* Look for bed-style names. */
if (fitFields(hash, "chrom", "chromStart", "chromEnd", retChrom, retStart, retEnd))
    {
    fitField(hash, "name", retName);
    fitField(hash, "score", retScore);
    fitField(hash, "strand", retStrand);
    fitField(hash, "thickStart", retCdsStart);
    fitField(hash, "thickEnd", retCdsEnd);
    fitField(hash, "blockCount", retCount);
    fitField(hash, "chromStarts", retStarts) ||
	fitField(hash, "blockStarts", retStarts);
    fitField(hash, "blockSizes", retEndsSizes);
    }
/* Look for psl-style names. */
else if (fitFields(hash, "tName", "tStart", "tEnd", retChrom, retStart, retEnd))
    {
    fitField(hash, "qName", retName);
    fitField(hash, "strand", retStrand);
    retScore[0] = 0;
    retCdsStart[0] = 0;
    retCdsEnd[0] = 0;
    fitField(hash, "blockCount", retCount);
    fitField(hash, "tStarts", retStarts);
    fitField(hash, "blockSizes", retEndsSizes);
    }
/* Look for gene prediction names. */
else if (fitFields(hash, "chrom", "txStart", "txEnd", retChrom, retStart, retEnd))
    {
    fitField(hash, "geneName", retName) ||  // tweak for refFlat type
	fitField(hash, "name", retName);
    fitField(hash, "score", retScore);      // some variants might have it...
    fitField(hash, "strand", retStrand);
    fitField(hash, "cdsStart", retCdsStart);
    fitField(hash, "cdsEnd", retCdsEnd);
    fitField(hash, "exonCount", retCount);
    fitField(hash, "exonStarts", retStarts);
    fitField(hash, "exonEnds", retEndsSizes);
    }
/* Look for repeatMasker names. */
else if (fitFields(hash, "genoName", "genoStart", "genoEnd", retChrom, retStart, retEnd))
    {
    fitField(hash, "repName", retName);
    fitField(hash, "swScore", retScore);
    fitField(hash, "strand", retStrand);
    retCdsStart[0] = 0;
    retCdsEnd[0] = 0;
    retCount[0] = 0;
    retStarts[0] = 0;
    retEndsSizes[0] = 0;
    }
else if (startsWith("chr", table) && endsWith(table, "_gl") && hashLookup(hash, "start") && hashLookup(hash, "end"))
    {
    strcpy(retChrom, "");
    strcpy(retStart, "start");
    strcpy(retEnd, "end");
    fitField(hash, "frag", retName);
    fitField(hash, "strand", retStrand);
    retScore[0] = 0;
    retCdsStart[0] = 0;
    retCdsEnd[0] = 0;
    retCount[0] = 0;
    retStarts[0] = 0;
    retEndsSizes[0] = 0;
    }
else
    gotIt = FALSE;
freeHash(&hash);
if (sameString(db, hGetDb()))
    hFreeConn(&conn);
else
    hFreeConn2(&conn);
*retBinned = binned;
return gotIt;
}


boolean hFindBed12Fields(char *table, 
	char retChrom[32], char retStart[32], char retEnd[32],
	char retName[32], char retScore[32], char retStrand[32],
        char retCdsStart[32], char retCdsEnd[32],
	char retCount[32], char retStarts[32], char retEndsSizes[32])
/* Given a table return the fields corresponding to all the bed 12 
 * fields, if they exist.  Fields that don't exist in the given table 
 * will be set to "". */
{
boolean isBinned;
return hFindBed12FieldsAndBinDb(hGetDb(), table,
				retChrom, retStart, retEnd,
				retName, retScore, retStrand,
				retCdsStart, retCdsEnd,
				retCount, retStarts, retEndsSizes,
				&isBinned);
}

boolean hFindBed12FieldsDb(char *db, char *table, 
	char retChrom[32], char retStart[32], char retEnd[32],
	char retName[32], char retScore[32], char retStrand[32],
        char retCdsStart[32], char retCdsEnd[32],
	char retCount[32], char retStarts[32], char retEndsSizes[32])
/* Given a table return the fields corresponding to all the bed 12 
 * fields, if they exist.  Fields that don't exist in the given table 
 * will be set to "". */
{
boolean isBinned;
return hFindBed12FieldsAndBinDb(db, table,
				retChrom, retStart, retEnd,
				retName, retScore, retStrand,
				retCdsStart, retCdsEnd,
				retCount, retStarts, retEndsSizes,
				&isBinned);
}

boolean hFindFieldsAndBin(char *table, 
	char retChrom[32], char retStart[32], char retEnd[32],
	boolean *retBinned)
/* Given a table return the fields for selecting chromosome, start, end,
 * and whether it's binned . */
{
char retName[32];
char retScore[32];
char retStrand[32];
char retCdsStart[32];
char retCdsEnd[32];
char retCount[32];
char retStarts[32];
char retEndsSizes[32];
return hFindBed12FieldsAndBinDb(hGetDb(), table,
				retChrom, retStart, retEnd,
				retName, retScore, retStrand,
				retCdsStart, retCdsEnd,
				retCount, retStarts, retEndsSizes,
				retBinned);
}

boolean hFindChromStartEndFields(char *table, 
	char retChrom[32], char retStart[32], char retEnd[32])
/* Given a table return the fields for selecting chromosome, start, and end. */
{
char retName[32];
char retScore[32];
char retStrand[32];
char retCdsStart[32];
char retCdsEnd[32];
char retCount[32];
char retStarts[32];
char retEndsSizes[32];
boolean isBinned;
return hFindBed12FieldsAndBinDb(hGetDb(), table,
				retChrom, retStart, retEnd,
				retName, retScore, retStrand,
				retCdsStart, retCdsEnd,
				retCount, retStarts, retEndsSizes,
				&isBinned);
}


boolean hFindChromStartEndFieldsDb(char *db, char *table, 
	char retChrom[32], char retStart[32], char retEnd[32])
/* Given a table return the fields for selecting chromosome, start, and end. */
{
char retName[32];
char retScore[32];
char retStrand[32];
char retCdsStart[32];
char retCdsEnd[32];
char retCount[32];
char retStarts[32];
char retEndsSizes[32];
boolean isBinned;
return hFindBed12FieldsAndBinDb(db, table,
				retChrom, retStart, retEnd,
				retName, retScore, retStrand,
				retCdsStart, retCdsEnd,
				retCount, retStarts, retEndsSizes,
				&isBinned);
}


struct hTableInfo *hFindTableInfoDb(char *db, char *chrom, char *rootName)
/* Find table information.  Return NULL if no table. */
{
static struct hash *hash;
struct hTableInfo *hti;
char fullName[64];
boolean isSplit = FALSE;

if (chrom == NULL)
    chrom = "chr1";
if (hash == NULL)
    hash = newHash(7);
if ((hti = hashFindVal(hash, rootName)) == NULL)
    {
    sprintf(fullName, "%s_%s", chrom, rootName);
    if (hTableExists(fullName))
	isSplit = TRUE;
    else
        {
	strcpy(fullName, rootName);
	if (!hTableExists(fullName))
	    return NULL;
	}
    AllocVar(hti);
    hashAddSaveName(hash, rootName, hti, &hti->rootName);
    hti->isSplit = isSplit;
    hti->isPos = hFindBed12FieldsAndBinDb(db, fullName,
	hti->chromField, hti->startField, hti->endField,
	hti->nameField, hti->scoreField, hti->strandField,
	hti->cdsStartField, hti->cdsEndField,
	hti->countField, hti->startsField, hti->endsSizesField,
	&hti->hasBin);
    hti->hasCDS = (hti->cdsStartField[0] != 0);
    hti->hasBlocks = (hti->startsField[0] != 0);
    if (hti->isPos)
	{
	if (sameString(hti->startsField, "exonStarts"))
	    hti->type = cloneString("genePred");
	else if (sameString(hti->startsField, "chromStarts") ||
		 sameString(hti->startsField, "blockStarts"))
	    hti->type = cloneString("bed 12");
	else if (sameString(hti->startsField, "tStarts"))
	    hti->type = cloneString("psl");
	else if (hti->cdsStartField[0] != 0)
	    hti->type = cloneString("bed 8");
	else if (hti->strandField[0] !=0  &&  hti->chromField[0] == 0)
	    hti->type = cloneString("gl");
	else if (hti->strandField[0] !=0)
	    hti->type = cloneString("bed 6");
	else if (hti->nameField[0] !=0)
	    hti->type = cloneString("bed 4");
	else
	    hti->type = cloneString("bed 3");
	}
    else
	hti->type = NULL;
    }
return hti;
}


struct hTableInfo *hFindTableInfo(char *chrom, char *rootName)
/* Find table information.  Return NULL if no table. */
{
return hFindTableInfoDb(hGetDb(), chrom, rootName);
}


boolean hFindSplitTable(char *chrom, char *rootName, 
	char retTableBuf[64], boolean *hasBin)
/* Find name of table that may or may not be split across chromosomes. 
 * Return FALSE if table doesn't exist.  */
{
struct hTableInfo *hti = hFindTableInfo(chrom, rootName);
if (hti == NULL)
    return FALSE;
if (retTableBuf != NULL)
    {
    if (hti->isSplit)
	snprintf(retTableBuf, 64, "%s_%s", chrom, rootName);
    else
	strncpy(retTableBuf, rootName, 64);
    }
if (hasBin != NULL)
    *hasBin = hti->hasBin;
return TRUE;
}

boolean hIsMgscHost()
/* Return TRUE if this is running on web server only
 * accessible to Mouse Genome Sequencing Consortium. */
{
static boolean gotIt = FALSE;
static boolean priv = FALSE;
if (!gotIt)
    {
    char *t = getenv("HTTP_HOST");
    if (t != NULL && (startsWith("hgwdev-mgsc", t)))
        priv = TRUE;
    gotIt = TRUE;
    }
return priv;
}

boolean hIsPrivateHost()
/* Return TRUE if this is running on private web-server. */
{
static boolean gotIt = FALSE;
static boolean priv = FALSE;
if (!gotIt)
    {
    char *t = getenv("HTTP_HOST");
    if (t != NULL && (startsWith("genome-test", t) || startsWith("hgwdev", t)))
        priv = TRUE;
    gotIt = TRUE;
    }
return priv;
}


int hOffsetPastBin(char *chrom, char *table)
/* Return offset into a row of table that skips past bin
 * field if any. */
{
struct hTableInfo *hti = hFindTableInfo(chrom, table);
if (hti == NULL)
    return 0;
return hti->hasBin;
}

/* Stuff to handle binning - which helps us restrict our
 * attention to the parts of database that contain info
 * about a particular window on a chromosome. This scheme
 * will work without modification for chromosome sizes up
 * to half a gigaBase.  The finest sized bin is 128k (1<<17).
 * The next coarsest is 8x as big (1<<3).  There's a hierarchy
 * of bins with the chromosome itself being the final bin.
 * Features are put in the finest bin they'll fit in. */

int hFindBin(int start, int end)
/* Given start,end in chromosome coordinates assign it
 * a bin.   There's a bin for each 128k segment, for each
 * 1M segment, for each 8M segment, for each 64M segment,
 * and for each chromosome (which is assumed to be less than
 * 512M.)  A range goes into the smallest bin it will fit in. */
{
return binFromRange(start, end);
}

void hAddBinToQuery(int start, int end, struct dyString *query)
/* Add clause that will restrict to relevant bins to query. */
{
int bFirstShift = binFirstShift(), bNextShift = binNextShift();
int startBin = (start>>bFirstShift), endBin = ((end-1)>>bFirstShift);
int i, levels = binLevels();

dyStringAppend(query, "(");
for (i=0; i<levels; ++i)
    {
    int offset = binOffset(i);
    if (i != 0)
        dyStringAppend(query, " or ");
    if (startBin == endBin)
        dyStringPrintf(query, "bin=%u", startBin + offset);
    else
        dyStringPrintf(query, "bin>=%u and bin<=%u", 
		startBin + offset, endBin + offset);
    startBin >>= bNextShift;
    endBin >>= bNextShift;
    }
dyStringAppend(query, ")");
dyStringAppend(query, " and ");
}

static struct sqlResult *hExtendedRangeQuery(struct sqlConnection *conn,
	char *rootTable, char *chrom,
	int start, int end, char *extraWhere, int *retRowOffset, boolean order)
/* Construct and make a query to tables that may be split and/or
 * binned. */
{
struct hTableInfo *hti = hFindTableInfo(chrom, rootTable);
struct sqlResult *sr = NULL;
struct dyString *query = newDyString(1024);
char fullTable[64], *table = NULL;
int rowOffset = 0;

if (hti == NULL)
    {
    warn("table %s doesn't exist", rootTable);
    }
else
    {
    dyStringAppend(query, "select * from ");
    if (hti->isSplit)
	{
	char fullTable[64];
	sprintf(fullTable, "%s_%s", chrom, rootTable);
	if (!sqlTableExists(conn, fullTable))
	     warn("%s doesn't exist", fullTable);
	else
	    {
	    table = fullTable;
	    dyStringPrintf(query, "%s where ", table);
	    }
	}
    else
        {
	table = rootTable;
	dyStringPrintf(query, "%s where %s='%s' and ", 
	    table, hti->chromField, chrom);
	}
    }
if (table != NULL)
    {
    if (hti->hasBin)
        {
	hAddBinToQuery(start, end, query);
	rowOffset = 1;
	}
    dyStringPrintf(query, "%s<%u and %s>%u", 
    	hti->startField, end, hti->endField, start);
    if (extraWhere)
        dyStringPrintf(query, " and %s", extraWhere);
    if (order)
        dyStringPrintf(query, " order by %s", hti->startField);
    sr = sqlGetResult(conn, query->string);
    }
freeDyString(&query);
*retRowOffset = rowOffset;
return sr;
}

struct sqlResult *hRangeQuery(struct sqlConnection *conn,
	char *rootTable, char *chrom,
	int start, int end, char *extraWhere, int *retRowOffset)
/* Construct and make a query to tables that may be split and/or
 * binned. */
{
return hExtendedRangeQuery(conn, rootTable, chrom, start, end, 
	extraWhere, retRowOffset, FALSE);
}

struct sqlResult *hOrderedRangeQuery(struct sqlConnection *conn,
	char *rootTable, char *chrom,
	int start, int end, char *extraWhere, int *retRowOffset)
/* Construct and make a query to tables that may be split and/or
 * binned. Forces return values to be sorted by chromosome start. */
{
return hExtendedRangeQuery(conn, rootTable, chrom, start, end, 
	extraWhere, retRowOffset, TRUE);
}


struct sqlResult *hChromQuery(struct sqlConnection *conn,
	char *rootTable, char *chrom,
	char *extraWhere, int *retRowOffset)
/* Construct and make a query across whole chromosome to tables 
 * that may be split and/or
 * binned. */
{
struct hTableInfo *hti = hFindTableInfo(chrom, rootTable);
struct sqlResult *sr = NULL;
struct dyString *query = newDyString(1024);
char fullTable[64], *table = NULL;
int rowOffset = 0;

if (hti == NULL)
    {
    warn("table %s doesn't exist", rootTable);
    }
else
    {
    rowOffset = hti->hasBin;
    if (hti->isSplit)
	{
        dyStringPrintf(query, "select * from %s_%s", chrom, rootTable);
	if (extraWhere != NULL)
	    dyStringPrintf(query, " where %s", extraWhere);
	}
    else
	{
        dyStringPrintf(query, "select * from %s where %s='%s'", rootTable,
		hti->chromField, chrom);
	if (extraWhere != NULL)
	    dyStringPrintf(query, " and (%s)", extraWhere);
	}
    sr = sqlGetResult(conn, query->string);
    }
freeDyString(&query);
*retRowOffset = rowOffset;
return sr;
}

boolean hTrackOnChrom(struct trackDb *tdb, char *chrom)
/* Return TRUE if track exists on this chromosome. */
{
boolean chromOk = TRUE;
char splitTable[64];
if (tdb->restrictCount > 0)
    chromOk =  (stringArrayIx(chrom, tdb->restrictList, tdb->restrictCount) >= 0);
return (chromOk && 
	hFindSplitTable(chrom, tdb->tableName, splitTable, NULL) &&
	!sameString(splitTable, "mrna")	 /* Long ago we reused this name badly. */
	);
}

struct trackDb *hTrackDb(char *chrom)
/* Load tracks associated with current chromosome (which may
 * be NULL */
{
struct sqlConnection *conn = hAllocConn();
struct trackDb *tdbList = NULL, *tdb;
boolean privateToo = hIsPrivateHost();
struct sqlResult *sr;
char **row;
char *database = hGetDb();
char query[256];
char *trackDb = hTrackDbName();
if(hdbTrackDb == NULL)
    errAbort("Please contact the system administrator to set the hg.trackDb in hg.conf");
snprintf(query, sizeof(query), "select * from %s", trackDb);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    boolean keeper = FALSE;
    tdb = trackDbLoad(row);
    hLookupStringsInTdb(tdb, database);
    if (!tdb->private || privateToo)
	{
	if (hTrackOnChrom(tdb, chrom))
	    {
	    slAddHead(&tdbList, tdb);
	    keeper = TRUE;
	    }
	}
    if (!keeper)
       trackDbFree(&tdb);
    }
freez(&trackDb);
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&tdbList);
return tdbList;
}

boolean hgParseChromRange(char *spec, char **retChromName, 
	int *retWinStart, int *retWinEnd)
/* Parse something of form chrom:start-end into pieces. */
{
char *chrom, *start, *end;
char buf[256];
int iStart, iEnd;

strncpy(buf, spec, 256);
chrom = buf;
start = strchr(chrom, ':');

if (start == NULL)
    {
    /* If just chromosome name cover all of it. */
    if ((chrom = hgOfficialChromName(chrom)) == NULL)
	return FALSE;
    else
       {
       chrom;
       iStart = 0;
       iEnd = hChromSize(chrom);
       }
    }
else 
    {
    *start++ = 0;
    end = strchr(start, '-');
    if (end == NULL)
	return FALSE;
    else
    *end++ = 0;
    chrom = trimSpaces(chrom);
    start = trimSpaces(start);
    end = trimSpaces(end);
    if (!isdigit(start[0]))
	return FALSE;
    if (!isdigit(end[0]))
	return FALSE;
    if ((chrom = hgOfficialChromName(chrom)) == NULL)
	return FALSE;
    iStart = atoi(start)-1;
    iEnd = atoi(end);
    }
if (retChromName != NULL)
    *retChromName = chrom;
if (retWinStart != NULL)
    *retWinStart = iStart;
if (retWinEnd != NULL)
    *retWinEnd = iEnd;
return TRUE;
}

boolean hgIsChromRange(char *spec)
/* Returns TRUE if spec is chrom:N-M for some human
 * chromosome chrom and some N and M. */
{
return hgParseChromRange(spec, NULL, NULL, NULL);
}

#ifdef UNUSED
boolean hgParseContigRange(char *spec, char **retChromName, 
	int *retWinStart, int *retWinEnd)
/* Parse something of form contig:start-end into pieces. */
{
char *contig, *start,*end;
char buf[256];
char contigName[256];
int iStart, iEnd;
size_t colinspot;
char *chrom;
int contigstart,contigend;
int spot;

strncpy(buf, spec, 256);
contig = buf;
start = strchr(contig, ':');
if (start == NULL)
    return FALSE;
if (startsWith("ctg",contig) == FALSE && startsWith("NT_", contig) == FALSE)
    return FALSE;
spot = strcspn(contig,":");
strncpy(contigName,contig,spot);
contigName[spot] = '\0';
*start++ = 0;
end = strchr(start, '-');
if (end == NULL)
    return FALSE;
else
*end++ = 0;
contig = trimSpaces(contig);
start = trimSpaces(start);
end = trimSpaces(end);
if (!isdigit(start[0]))
    return FALSE;
if (!isdigit(end[0]))
    return FALSE;
iStart = atoi(start)-1;
iEnd = atoi(end);
if (retChromName != NULL)
    *retChromName = chrom;
if (retWinStart != NULL)
    *retWinStart = contigstart + iStart;
if (retWinEnd != NULL)
    *retWinEnd = contigstart + iEnd;
return TRUE; 
}

boolean hgIsContigRange(char *spec)
/* Returns TRUE if spec is chrom:N-M for some human
 * chromosome chrom and some N and M. */
{
return hgParseContigRange(spec, NULL, NULL, NULL);
}  
#endif /* UNUSED */

struct trackDb *hMaybeTrackInfo(struct sqlConnection *conn, char *trackName)
/* Look up track in database, return NULL if it's not there. */
{
char query[256];
struct sqlResult *sr;
char **row;
struct trackDb *tdb;

sprintf(query, "select * from %s where tableName = '%s'", hTrackDbName(), trackName);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    return NULL;
tdb = trackDbLoad(row);
hLookupStringsInTdb(tdb, hGetDb());
sqlFreeResult(&sr);
return tdb;
}

struct trackDb *hTrackInfo(struct sqlConnection *conn, char *trackName)
/* Look up track in database, errAbort if it's not there. */
{
struct trackDb *tdb;

tdb = hMaybeTrackInfo(conn, trackName);
if (tdb == NULL)
    errAbort("Track %s not found", trackName);
return tdb;
}

struct dbDb *hGetIndexedDatabases()
/* Get list of databases for which there is a nib dir. 
 * Dispose of this with dbDbFreeList. */
{
struct sqlConnection *conn = hConnectCentral();
struct sqlResult *sr = NULL;
char **row;
struct dbDb *dbList = NULL, *db;

/* Scan through dbDb table, loading into list */
sr = sqlGetResult(conn, "select * from dbDb where active = 1 order by orderKey");
while ((row = sqlNextRow(sr)) != NULL)
    {
    db = dbDbLoad(row);
    slAddHead(&dbList, db);
    }
sqlFreeResult(&sr);
hDisconnectCentral(&conn);
slReverse(&dbList);
return dbList;
}


struct dbDb *hGetAxtInfoDbs()
/* Get list of db's where we have axt files listed in axtInfo . 
 * The db's with the same organism as current db go last.
 * Dispose of result with dbDbFreeList. */
{
struct dbDb *dbDbList = NULL, *dbDb;
struct hash *hash = hashNew(7); // 2^^7 entries = 128
struct slName *dbNames = NULL, *dbName;
struct dyString *query = newDyString(256);
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
char *organism = hOrganism(hdbName);
int count;

if (! hTableExists("axtInfo"))
    {
    dyStringFree(&query);
    hashFree(&hash);
    hFreeConn(&conn);
    return NULL;
    }

/* "species" is a misnomer, we're really looking up database names. */
sr = sqlGetResult(conn, "select species from axtInfo");
while ((row = sqlNextRow(sr)) != NULL)
    {
    // uniquify database names
    if (hashLookup(hash, row[0]) == NULL)
	{
	struct slName *sln = newSlName(cloneString(row[0]));
	slAddHead(&dbNames, sln);
	hashStoreName(hash, cloneString(row[0]));
	}
    }
sqlFreeResult(&sr);
hFreeConn(&conn);

/* Traverse the uniquified list of databases twice: first for db's with 
 * a different organism, then for db's with this organism. */
conn = hConnectCentral();
dyStringClear(query);
dyStringAppend(query, "SELECT * from dbDb");
count = 0;
for (dbName = dbNames;  dbName != NULL;  dbName = dbName->next)
    {
    char *dbOrg = hOrganism(dbName->name);
    if (! sameString(dbOrg, organism))
	{
	count++;
	if (count == 1)
	    dyStringPrintf(query, " where active = 1 and (name = '%s'",
			   dbName->name);
	else
	    dyStringPrintf(query, " or name = '%s'", dbName->name);
	}
    }
dyStringPrintf(query, ") order by orderKey");
if (count > 0)
    {
    sr = sqlGetResult(conn, query->string);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	dbDb = dbDbLoad(row);
	slAddHead(&dbDbList, dbDb);
	}
    sqlFreeResult(&sr);
    }
dyStringClear(query);
dyStringAppend(query, "SELECT * from dbDb");
count = 0;
for (dbName = dbNames;  dbName != NULL;  dbName = dbName->next)
    {
    char *dbOrg = hOrganism(dbName->name);
    if (sameString(dbOrg, organism))
	{
	count++;
	if (count == 1)
	    dyStringPrintf(query, " where active = 1 and (name = '%s'",
			   dbName->name);
	else
	    dyStringPrintf(query, " or name = '%s'", dbName->name);
	}
    }
dyStringPrintf(query, ") order by orderKey");
if (count > 0)
    {
    sr = sqlGetResult(conn, query->string);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	dbDb = dbDbLoad(row);
	slAddHead(&dbDbList, dbDb);
	}
    sqlFreeResult(&sr);
    }
hDisconnectCentral(&conn);
slFreeList(&dbNames);
dyStringFree(&query);
hashFree(&hash);

slReverse(&dbDbList);
return(dbDbList);
}

struct axtInfo *hGetAxtAlignments(char *db)
/* Get list of alignments where we have axt files listed in axtInfo . 
 * Dispose of this with axtInfoFreeList. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row;
struct axtInfo *aiList = NULL, *ai;
char query[256];

sprintf(query, "select * from axtInfo where species = '%s' and chrom = 'chr1'",db);
/* Scan through axtInfo table, loading into list */
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    ai = axtInfoLoad(row);
    slAddHead(&aiList, ai);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&aiList);
return aiList;
}


struct dbDb *hGetBlatIndexedDatabases()
/* Get list of databases for which there is a BLAT index. 
 * Dispose of this with dbDbFreeList. */
{
struct hash *hash=newHash(5);
struct sqlConnection *conn = hConnectCentral();
struct sqlResult *sr;
char **row;
struct dbDb *dbList = NULL, *db;

/* Get hash of active blat servers. */
sr = sqlGetResult(conn, "select db from blatServers");
while ((row = sqlNextRow(sr)) != NULL)
    hashAdd(hash, row[0], NULL);
sqlFreeResult(&sr);

/* Scan through dbDb table, keeping ones that are indexed. */
sr = sqlGetResult(conn, "select * from dbDb");
while ((row = sqlNextRow(sr)) != NULL)
    {
    db = dbDbLoad(row);
    if (hashLookup(hash, db->name))
        {
	slAddHead(&dbList, db);
	}
    else
        dbDbFree(&db);
    }
sqlFreeResult(&sr);
hDisconnectCentral(&conn);
hashFree(&hash);
slReverse(&dbList);
return dbList;
}

boolean hIsBlatIndexedDatabase(char *db)
/* Return TRUE if have a BLAT server on sequence corresponding 
 * to give database. */
{
struct sqlConnection *conn = hConnectCentral();
boolean gotIx;
char query[256];

sprintf(query, "select name from dbDb where name = '%s'", db);
gotIx = sqlExists(conn, query);
hDisconnectCentral(&conn);
return gotIx;
}

struct blatServerTable *hFindBlatServer(char *db, boolean isTrans)
/* Return server for given database.  Db can either be
 * database name or description. Ponter returned is owned
 * by this function and shouldn't be modified */
{
static struct blatServerTable st;
struct sqlConnection *conn = hConnectCentral();
char query[256];
struct sqlResult *sr;
char **row;
char dbActualName[32];

/* If necessary convert database description to name. */
sprintf(query, "select name from dbDb where name = '%s'", db);
if (!sqlExists(conn, query))
    {
    sprintf(query, "select name from dbDb where description = '%s'", db);
    if (sqlQuickQuery(conn, query, dbActualName, sizeof(dbActualName)) != NULL)
        db = dbActualName;
    }

/* Do a little join to get data to fit into the blatServerTable. */
sprintf(query, "select dbDb.name,dbDb.description,blatServers.isTrans"
               ",blatServers.host,blatServers.port,dbDb.nibPath "
	       "from dbDb,blatServers where blatServers.isTrans = %d and "
	       "dbDb.name = '%s' and dbDb.name = blatServers.db", 
	       isTrans, db);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    {
    errAbort("Can't find a server for %s database %s\n",
	    (isTrans ? "translated" : "DNA"), db);
    }
st.db = cloneString(row[0]);
st.genome = cloneString(row[1]);
st.isTrans = atoi(row[2]);
st.host = cloneString(row[3]);
st.port = cloneString(row[4]);
st.nibDir = cloneString(row[5]);
sqlFreeResult(&sr);
hDisconnectCentral(&conn);
return &st;
}

char *hDefaultDbForOrganism(char *organism)
/*
Purpose: Return the default database matching the organism.

param organism - The organism for which we are trying to get the 
    default database.
return - The default database name for this organism
 */
{
char *result = hGetDb();

if (strstrNoCase(organism, "mouse"))
    {
    result = defaultMouse;
    }
else if (strstrNoCase(organism, "zoo"))
    {
    result = defaultZoo;
    }
else if (strstrNoCase(organism, "human"))
    {
    result = defaultHuman;
    }
else if (strstrNoCase(organism, "rat"))
    {
    result = defaultRat;
    }

return result;
}
