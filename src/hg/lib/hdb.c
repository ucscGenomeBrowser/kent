/* hdb - human genome browser database. */
#include "common.h"
#include "portable.h"
#include "hash.h"
#include "jksql.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nib.h"
#include "hdb.h"
#include "hgRelate.h"
#include "fa.h"
#include "hgConfig.h"
#include "ctgPos.h"

static struct sqlConnCache *hdbCc = NULL;

struct dbConv
/* Pair for converting database to assembly and vice-versa */
{
    char *db;        /* database name, i.e. "hg6" */
    char *freeze;    /* assembly freeze, i.e. "Dec. 12, 2000" */
};

static struct dbConv dbTable[] = {
    {"hg3","July 17, 2000"},
    {"hg4","Sept. 5, 2000"},
    {"hg5","Oct. 7, 2000"},
    {"hg6","Dec. 12, 2000"},
    {"hg7","April 1, 2001"},
//    {"hg8","Aug. 6, 2001"},
};

static char *hdbHost;
static char *hdbName = "hg6";
static char *hdbUser;
static char *hdbPassword;

void hDefaultConnect()
/* read in the connection options from config file */
{
hdbHost 	= cfgOption("db.host");
hdbUser 	= cfgOption("db.user");
hdbPassword	= cfgOption("db.password");

if(hdbHost == NULL || hdbUser == NULL || hdbPassword == NULL)
	errAbort("cannot read in connection setting from configuration file.");
}

void hSetDbConnect(char* host, char *db, char *user, char *password)
/* set the connection information for the database */
{
	hdbHost = host;
	hdbName = db;
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

char *hGetDb()
/* Return the current database name. */
{
return hdbName;
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
if (hdbCc == NULL)
    hdbCc = sqlNewConnCache(hdbName);
return sqlAllocConnection(hdbCc);
}

struct sqlConnection *hFreeConn(struct sqlConnection **pConn)
/* Put back connection for reuse. */
{
sqlFreeConnection(hdbCc, pConn);
}

boolean hTableExists(char *table)
/* Return TRUE if a table exists in database. */
{
struct sqlConnection *conn = hAllocConn();
boolean exists = sqlTableExists(conn, table);
hFreeConn(&conn);
return exists;
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

struct dnaSeq *hDnaFromSeq(char *seqName, int start, int end, enum dnaCase dnaCase)
/* Fetch DNA */
{
char fileName[512];
struct dnaSeq *seq;

hNibForChrom(seqName, fileName);
seq = nibLoadPart(fileName, start, end-start);
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
        errAbort("External file %s has changed, need to resync database", path);
    lsf->id = extId;
    if ((lsf->fd = open(path, O_RDONLY)) < 0)
        errAbort("Couldn't open external file %s", path);
    slAddHead(&largeFileList, lsf);
    sqlFreeResult(&sr);
    hgFreeConn(&conn);
    return lsf;
    }
}

void *readOpenFileSection(int fd, long offset, size_t size, char *fileName)
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
long offset;
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


char *hFreezeFromDb(char *database)
/* return the freeze for the database version. 
   For example: "hg6" returns "Dec 12, 2000". If database
   not recognized returns NULL */
{
int i;
for(i=0;i<ArraySize(dbTable);i++)
    if(sameString(database,dbTable[i].db))
	return cloneString(dbTable[i].freeze);
return NULL;
}

char *hDbFromFreeze(char *freeze)
/* Return database version from freeze name. */
{
int i;
for(i=0;i<ArraySize(dbTable);i++)
    if(sameString(freeze,dbTable[i].freeze))
	return cloneString(dbTable[i].db);
return NULL;
}

struct slName *hDbList()
/* List of all database versions. */
{
struct slName *nList = NULL, *n;
int i;
for(i=2;i<ArraySize(dbTable);i++)
    {
    n = newSlName(dbTable[i].db);
    slAddTail(&nList, n);
    }
return nList;
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

boolean hFindChromStartEndFields(char *table, 
	char retChrom[32], char retStart[32], char retEnd[32])
/* Given a table return the fields for selecting chromosome, start, and end. */
{
char query[256];
struct sqlResult *sr;
char **row;
struct hash *hash = newHash(5);
struct sqlConnection *conn = hAllocConn();
boolean gotIt = TRUE;

/* Read table description into hash. */
sprintf(query, "describe %s", table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hashAdd(hash, row[0], NULL);
    }
sqlFreeResult(&sr);

/* Look for bed-style names. */
if (fitFields(hash, "chrom", "chromStart", "chromEnd", retChrom, retStart, retEnd))
    ;
/* Look for psl-style names. */
else if (fitFields(hash, "tName", "tStart", "tEnd", retChrom, retStart, retEnd))
    ;
/* Look for gene prediction names. */
else if (fitFields(hash, "chrom", "txStart", "txEnd", retChrom, retStart, retEnd))
    ;
/* Look for repeatMasker names. */
else if (fitFields(hash, "genoName", "genoStart", "genoEnd", retChrom, retStart, retEnd))
    ;
else
    gotIt = FALSE;
freeHash(&hash);
hFreeConn(&conn);
return gotIt;
}

