/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* HGAP - Human Genome Annotation Project database interface routines. 
 * Queries database, handles coordinate translation. */
#include "common.h"
#include "portable.h"
#include "localmem.h"
#include "dystring.h"
#include "hash.h"
#include "fa.h"
#include "jksql.h"
#include "hgap.h"

static struct sqlConnCache *hgcc = NULL;

char *hgDbName = "h";

void hgSetDb(char *dbName)
/* Set the database name. */
{
if (hgcc != NULL)
    errAbort("Can't hgSetDb after an hgAllocConn(), sorry.");
hgDbName = dbName;
}

char *hgGetDb()
/* Return the current database name. */
{
return hgDbName;
}

struct sqlConnection *hgAllocConn()
/* Get free connection if possible. If not allocate a new one. */
{
if (hgcc == NULL)
    hgcc = sqlNewConnCache(hgDbName);
return sqlAllocConnection(hgcc);
}

struct sqlConnection *hgFreeConn(struct sqlConnection **pConn)
/* Put back connection for reuse. */
{
sqlFreeConnection(hgcc, pConn);
}

HGID hgIdQuery(struct sqlConnection *conn, char *query)
/* Return first field of first table as HGID. 0 return ok. */
{
struct sqlResult *sr;
char **row;
HGID ret = 0;

sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row == NULL)
    errAbort("Couldn't find ID in response to %s\nDatabase needs updating", query);
ret = sqlUnsigned(row[0]);
sqlFreeResult(&sr);
return ret;
}

HGID hgRealIdQuery(struct sqlConnection *conn, char *query)
/* Return first field of first table as HGID- abort if 0. */
{
HGID ret = hgIdQuery(conn, query);
if (ret == 0)
    errAbort("Unexpected NULL response to %s", query);
return ret;
}


static HGID startUpdateId;	/* First ID in this update. */
static HGID endUpdateId;	/* One past last ID in this update. */

HGID hgNextId()
/* Get next free global ID. */
{
return endUpdateId++;
}

struct sqlConnection *hgStartUpdate()
/* This locks the update table for an update - which prevents anyone else
 * from updating and returns the first usable ID. */
{
struct sqlConnection *conn = sqlConnect(hgDbName);
startUpdateId = endUpdateId = hgIdQuery(conn, 
    "SELECT MAX(endId) from history");
return conn;
}

void hgEndUpdate(struct sqlConnection **pConn, char *comment, ...)
/* Finish up connection with a printf format comment. */
{
struct sqlConnection *conn = *pConn;
struct dyString *query = newDyString(256);
va_list args;
va_start(args, comment);

dyStringPrintf(query, "INSERT into history VALUES(NULL,%d,%d,USER(),\"", 
	startUpdateId, endUpdateId);
dyStringVaPrintf(query, comment, args);
dyStringAppend(query, "\",NOW())");
sqlUpdate(conn,query->string);
sqlDisconnect(pConn);
}

FILE *hgCreateTabFile(char *tableName)
/* Open a tab file with name corresponding to tableName.  This
 * may just be fclosed when done. (Currently just makes
 * tableName.tab in the current directory.) */
{
char fileName[256];
sprintf(fileName, "%s.tab", tableName);
return mustOpen(fileName, "w");
}

void hgLoadTabFile(struct sqlConnection *conn, char *tableName)
/* Load tab delimited file corresponding to tableName. */
{
char query[512];
char fileName[256];
sprintf(fileName, "%s.tab", tableName);
sprintf(query,
   "LOAD data local infile '%s' into table %s", fileName, tableName);
sqlUpdate(conn, query);
}





/* ----------------------------------------------------------------- */
struct hgNestBinder
/* Struct that associates a nest with a bac or contig. */
    {
    struct hgNest *nest;	/* Nest. */
    void *seqObj;		/* A bac, contig, etc. */
    };

static struct lm *hgLocalMem = NULL; /* Memory for bacs, contigs, nests, etc. */
struct hash *hgHash;		     /* Lookup by name of bacs, contigs, nests, etc. 
                                      * The val field of this is hgNestBinder. */

static struct hash *getHash()
/* Initialize local memory and hash tables. */
{
if (hgHash == NULL)
    hgHash = newHash(16);
return hgHash;
}

void *hgAlloc(size_t size)
/* Allocate memory from local pool. */
{
if (hgLocalMem == NULL)
    hgLocalMem = lmInit(16*1024);
return lmAlloc(hgLocalMem, size);
}

struct hgNestBinder *hgLookup(char *name)
/* See if we already have it... */
{
struct hashEl *hel;
if ((hel = hashLookup(getHash(), name)) == NULL)
    return NULL;
return hel->val;
}

void hgStore(char *name, struct hgNest *nest, void *seqObj)
/* Store association between name, nest, and object in hgHash. */
{
struct hgNestBinder *binder = hgAlloc(sizeof(*binder));
struct hashEl *hel;
binder->nest = nest;
binder->seqObj = seqObj;
hashAdd(getHash(), name, binder);
}

int cmpContigIx(const void *va, const void *vb)
/* Compare two hg. */
{
const struct hgContig *a = *((struct hgContig **)va);
const struct hgContig *b = *((struct hgContig **)vb);
return a->ix - b->ix;
}

static struct hgBac *hgBacIntoNest(char *acc)
/* Load BAC with given accession into memory.  Don't
 * free this, it is managed by system. */
{
struct hgBac *bac;
char query[256];
struct sqlConnection *conn;
struct sqlResult *sr;
HGID bacId;
HGID bacNestId;
char **row;
struct hgNest *bacNest;
struct hgContig *contig;
struct hgNest *contigNest;
struct hgNestBinder *binder;
int contigIx;

/* See if we already have it first. */
if ((binder = hgLookup(acc)) != NULL)
    return binder->seqObj;

/* Query database and load up basic fields for bac itself. */
conn = hgAllocConn();
sprintf(query, 
    "select id,fragments,ordering from bac where acc='%s'",
    acc);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row == NULL)
    errAbort("No clone with accession %s in database", acc);
bac = hgAlloc(sizeof(*bac));
bac->id = bacId = sqlUnsigned(row[0]);
if (strlen(acc) >= sizeof(bac->name))
    errAbort("accession %s too long", acc);
strcpy(bac->name, acc);
bac->contigCount = sqlUnsigned(row[1]);
bacNestId = sqlUnsigned(row[2]);
sqlFreeResult(&sr);

/* Create nest for bac and store results in hash table. */
sprintf(query,
    "select orientation,offset,length from ordering "
    "where id=%lu", bacNestId);
sr = sqlGetResult(conn, query);
if (sr == NULL)
    errAbort("No ordering in database for %s", acc);
row = sqlNextRow(sr);
bacNest = hgAlloc(sizeof(*bacNest));
bacNest->id = bacNestId;
bacNest->orientation = sqlSigned(row[0]);
bacNest->offset = sqlUnsigned(row[1]);
bacNest->size = sqlUnsigned(row[2]);
hgStore(acc, bacNest, bac);
bac->nest = bacNest;
sqlFreeResult(&sr);

/* Get relevant fields from database version of contig and
 * nest. */
sprintf(query,
 "select contig.id,contig.piece,contig.start,contig.size,"
         "ordering.id,ordering.orientation,ordering.offset "
 "from contig,ordering "
 "where contig.bac = %lu "
   "and ordering.contig = contig.id "
 "order by ordering.piece",
 bacId);
sr = sqlGetResult(conn, query);
if (sr == NULL)
    errAbort("No ordering in database for contigs of %s", acc);
while ((row = sqlNextRow(sr)) != NULL)
    {
    contig = hgAlloc(sizeof(*contig));
    contigNest = hgAlloc(sizeof(*contigNest));

    contig->nest = contigNest;
    contig->id = sqlUnsigned(row[0]);
    contig->ix = sqlUnsigned(row[1]);
    sprintf(contig->name, "%s.%d", acc, contig->ix);
    contig->bac = bac;
    contig->submitOffset = sqlUnsigned(row[2]);
    contig->size = sqlUnsigned(row[3]);
    slAddHead(&bac->contigs, contig);

    contigNest->parent = bacNest;
    contigNest->id = sqlUnsigned(row[4]);
    contigNest->orientation = sqlSigned(row[5]);
    contigNest->offset = sqlSigned(row[6]);
    contigNest->size = contig->size;
    contigNest->contig = contig;
    slAddHead(&bacNest->children, contigNest);

    hgStore(contig->name, contigNest, contig);
    }
slReverse(&bacNest->children);
slSort(&bac->contigs, cmpContigIx);
sqlFreeResult(&sr);
hgFreeConn(&conn);
return bac;
}

struct hgBac *hgGetBac(char *acc)
/* Load BAC with given accession into memory. Don't free this, it's
 * managed by system. */
{
return hgBacIntoNest(acc);
}

struct hgContig *hgGetContig(char *acc, int contigIx)
/* Get contig.  contigIx is position in submission, not submission in
 * ordering. contigIx starts with zero. */
{
struct hgBac *bac = hgGetBac(acc);
if (contigIx >= bac->contigCount)
    errAbort("contigIx out of range in hgGetContig");
return slElementFromIx(bac->contigs, contigIx);
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

void *readFileSection(char *fileName, long offset, size_t size)
/* Allocate a buffer big enough to hold a section of a file,
 * and read that section into it. */
{
int fd;
void *buf;
if ((fd = open(fileName, O_RDONLY)) < 0)
    errAbort("Couldn't open %s", fileName);
buf = readOpenFileSection(fd,offset,size,fileName);
close(fd);
return buf;
}

struct dnaSeq *hgContigSeq(struct hgContig *contig)
/* Return DNA associated with contig. */
{
char *buf;
struct dnaSeq *seq;
struct sqlConnection *conn = hgAllocConn();
HGID bacId = contig->bac->id;
char query[256];
struct sqlResult *sr;
char **row;
long offset;
size_t size;
static HGID lastBacId = 0;	/* Cache last file, it's frequently reused. */
static int bacFd = -1;	        /* Cache last file, it's frequently reused. */

if (lastBacId != bacId)
    {
    char fileName[512];
    char **row;

    if (bacFd >= 0)
	close(bacFd);
    sprintf(query, 
       "select extFile.path,seq.acc,contig.file_offset,contig.file_size "
       "from contig,seq,extFile "
       "where contig.id = %lu and seq.id = %lu and seq.extFile = extFile.id",
       contig->id, contig->bac->id);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    sprintf(fileName, "%s%s.fa", row[0], row[1]);
    offset = sqlUnsigned(row[2]);
    size = sqlUnsigned(row[3]);
    if ((bacFd = open(fileName, O_RDONLY)) < 0)
	errAbort("Couldn't open %s", fileName);
    lastBacId = bacId;
    }
else
    {
    sprintf(query, "select contig.file_offset,contig.file_size from contig "
                   "where contig.id = %lu", contig->id);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    offset = sqlUnsigned(row[0]);
    size = sqlUnsigned(row[1]);
    }
buf = readOpenFileSection(bacFd, offset, size, "bac");
seq = faFromMemText(buf);
sqlFreeResult(&sr);
hgFreeConn(&conn);
return seq;
}

struct largeSeqFile
/* Manages our large external sequence files.  Typically there will
 * be around four of these.  This basically caches the file handle
 * so don't have to keep opening and closing them. */
    {
    struct largeSeqFile *next;	/* Next in list. */
    char *path;		        /* Path name for file. */
    HGID id;			/* Id in extFile table. */
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
    sprintf(query, "select path,size from extFile where id=%lu", extId);
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

void hgRnaSeqAndId(char *acc, struct dnaSeq **retSeq, HGID *retId)
/* Return sequence for RNA and it's database ID. */
{
struct sqlConnection *conn = hgAllocConn();
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
hgFreeConn(&conn);
}

struct dnaSeq *hgExtSeq(char *acc)
/* Return externally stored sequence. */
{
struct dnaSeq *seq;
HGID id;
hgRnaSeqAndId(acc, &seq, &id);
return seq;
}

struct dnaSeq *hgRnaSeq(char *acc)
/* Return sequence for RNA. */
{
return hgExtSeq(acc);
}



static struct dnaSeq *getBacContigSeqList(struct sqlConnection *conn, char *acc)
/* Get list of contig DNA for bac. */
{
char query[256];
char extDir[3*128];
char path[512];

sprintf(query, 
    "select extFile.path from seq,extFile  "
    "where seq.acc = '%s' and seq.extFile = extFile.id", acc);
if (sqlQuickQuery(conn, query, extDir, sizeof(extDir)) == NULL)
    errAbort("Couldn't find bac %s in database", acc);
sprintf(path, "%s%s.fa", extDir, acc);
return faReadAllDna(path);
}

struct dnaSeq *hgBacContigSeq(char *acc)
/* Returns list of sequences, one for each contig in BAC. */
{
struct sqlConnection *conn = hgAllocConn();
struct dnaSeq *seqList = getBacContigSeqList(conn, acc);
hgFreeConn(&conn);
return seqList;
}


struct dnaSeq *hgBacOrderedSeq(struct hgBac *bac)
/* Return DNA associated with BAC including NNN's between
 * contigs in ordered coordinates. */
{
struct sqlConnection *conn = hgAllocConn();
struct dnaSeq *contigList, *contig;
struct sqlResult *sr;
char **row;
int totalSize = 0;
struct dnaSeq *seq;
int seqOffset = 0;
DNA *dna;
boolean firstTime = TRUE;
char query[256];

contigList = getBacContigSeqList(conn, bac->name);
totalSize = contigList->size;
for (contig = contigList->next; contig != NULL; contig = contig->next)
    totalSize += hgContigPad + contig->size;

AllocVar(seq);
seq->name = cloneString(bac->name);
seq->dna = dna = needLargeMem(totalSize+1);
seq->size = totalSize;
dna[totalSize] = 0;

sprintf(query, 
    "select contig.piece from contig,ordering "
    "where contig.bac = %lu "
    "and contig.id = ordering.contig "
    "order by ordering.piece", bac->id);
sr = sqlMustGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    int contigIx = sqlUnsigned(row[0]);
    struct dnaSeq *seq = slElementFromIx(contigList, contigIx);
    if (seq == NULL)
	errAbort("contig ix %d out of range in %s", contigIx, bac->name);
    if (firstTime)
	firstTime = FALSE;
    else
	{
	memset(dna+seqOffset, 'N', hgContigPad);
	seqOffset += hgContigPad;
	}
    memcpy(dna+seqOffset, seq->dna, seq->size);
    seqOffset += seq->size;
    }
sqlFreeResult(&sr);
freeDnaSeqList(&contigList);
hgFreeConn(&conn);
return seq;
}

struct dnaSeq *hgBacSubmittedSeq(char *acc)
/* Returns DNA associated with BAC in submitted ordering
 * and coordinates. */
{
struct sqlConnection *conn = hgAllocConn();
struct dnaSeq *seqList, *seq;
int totalSize = 0;
struct dnaSeq *res;
int seqOffset = 0;
DNA *dna;
boolean firstTime = TRUE;
struct hgBac *bac = hgBacIntoNest(acc);
struct hgContig *contig;
int prevEnd = 0;
int start = 0, size;

seqList = getBacContigSeqList(conn, bac->name);
for (contig = bac->contigs; contig != NULL; contig = contig->next)
    {
    if (contig->next == NULL)
	totalSize = contig->submitOffset + contig->size;
    }
AllocVar(res);
res->name = cloneString(acc);
res->size = totalSize;
res->dna = dna = needLargeMem(totalSize+1);
dna[totalSize] = 0;

for (contig = bac->contigs, seq=seqList; contig != NULL; contig = contig->next,seq = seq->next)
    {
    start = contig->submitOffset;
    size = contig->size;
    assert(start+size <= totalSize);
    memset(dna+prevEnd, 'N', start-prevEnd);
    memcpy(dna+start, seq->dna, size);
    prevEnd = start+size;
    }
freeDnaSeqList(&seqList);
hgFreeConn(&conn);
return res;
}

int hgOffset(struct hgNest *source, int offset, struct hgNest *dest)
/* Translate offset from source to destination coordinate space.
 * Destination has to be an ancestor (or the same) as source. */
{
struct hgNest *cur = source;
while (cur != dest)
    {
    if (cur == NULL)
	errAbort("Error missing ancestor in hgOffset");
    offset += cur->offset;
    cur = cur->parent;
    }
return offset;
}


