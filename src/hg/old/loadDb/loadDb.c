/* Load sequence into database. */
#include "common.h"
#include "errabort.h"
#include "memalloc.h"
#include "portable.h"
#include "hash.h"
#include "dystring.h"
#include "linefile.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "fuzzyFind.h"
#include "jksql.h"
#include "hgap.h"

char *nullPt = NULL;

char *database = "h";

char historyTable[] =	
/* This contains a row for each update made to database.
 * (The idea is that this is just updated in batch.)
 * It keeps track of which id global ids are used
 * as well as providing a record of updates. */
"create table history ("
  "ix int not null auto_increment primary key,"  /* Update number. */
  "startId int unsigned not null,"              /* Start this session's ids. */
  "endId int unsigned not null,"                /* First id for next session. */
  "who varchar(255) not null,"         /* User who updated. */
  "what varchar(255) not null,"        /* What they did. */
  "when timestamp not null)";	       /* When they did it. */

char extFileTable[] =
/* This keeps track of external files and directories. */
"create table extFile ("
  "id int unsigned not null primary key,"  /* Unique ID across all tables. */
  "name varchar(64) not null,"	  /* Symbolic name of file.  */
  "path varchar(255) not null,"   /* Full path. Ends in '/' if a dir. */
  "size int unsigned not null,"            /* Size of file (checked) */
                   /* Extra indices. */
  "unique (name))";

char seqTable[] =
/* This keeps track of a sequence. */
"create table seq ("
  "id int unsigned not null primary key," /* Unique ID across all tables. */
  "acc char(12) not null ,"	 /* GenBank accession number. */
  "size int unsigned not null,"           /* Size of sequence in bases. */
  "gb_date date not null,"       /* GenBank last modified date. */
  "extFile int unsigned not null,"       /* File it is in. */
  "file_offset int unsigned not null,"    /* Offset in file. */
  "file_size int unsigned not null,"      /* Size in file. */
	       /* Extra indices. */
  "unique (acc))";

char mrnaTable[] =
/* This keeps track of mRNA. */
"create table mrna ("
  "id int unsigned not null primary key,"          /* Id, same as seq ID. */
  "acc char(12) not null,"		  /* Genbank accession. */
  "type enum('EST','mRNA') not null,"	  /* Full length or EST. */
  "direction enum('5','3','0') not null," /* Read direction. */
  "source int unsigned not null,"	 	  /* Ref in source table. */
  "organism int unsigned not null," 		  /* Ref in organism table. */
  "library int unsigned not null,"		  /* Ref in library table. */
  "mrnaClone int unsigned not null,"              /* Ref in clone table. */
  "sex int unsigned not null,"                     /* Ref in sex table. */
  "tissue int unsigned not null,"                  /* Ref in tissue table. */
  "development int unsigned not null,"             /* Ref in development table. */
  "cell int unsigned not null,"                    /* Ref in cell table. */
  "cds int unsigned not null,"	                  /* Ref in CDS table. */
  "geneName int unsigned not null,"               /* Ref in geneName table. */
  "productName int unsigned not null,"            /* Ref in productName table. */
	   /* Extra indices. */
  "unique (acc),"
  "index (type),"
  "index (mrnaClone))";

char bacTable[] =
/* This keeps track of BAC's. */
"create table bac ("
  "id int unsigned not null primary key,"   /* Id. */
  "acc char(12) not null,"	            /* Genbank accession. */
  "version int unsigned not null,"          /* Genbank version number. */
  "source int unsigned not null,"           /* Ref in source table. */
  "organism int unsigned not null,"	    /* Ref in organism table. */
  "library int unsigned not null,"	    /* Ref in library table. */
  "bacClone int unsigned not null,"	    /* Ref in bacClone table. */
  "sex int unsigned not null,"              /* Ref in sex table. */
  "phase tinyint not null,"	            /* 0,1,2 or 3. */
  "center int unsigned not null,"           /* Ref in sequencing center table. */
  "fragments smallint not null,"            /* Number of contigs. */
  "cytoMap int unsigned not null,"          /* Ref in cytoMap table. */
  "chromosome int unsigned not null,"       /* Ref in chromosome table. */
  "ordering int unsigned not null,"         /* Ref in ordering table. */
	       /* Extra indices. */
  "unique (acc))";

char contigTable[] =
/* This keeps track of contigs.  Cordinates and order is as submitted to 
 * genBank.  This just tracks low level contigs - inside of a BAC clone.
 * For higher level stuff see ordering table. */
"create table contig ("
  "id int not null primary key,"    /* Id. */
  "bac int not null,"               /* BAC this is in. */
  "piece smallint not null,"        /* Submission order relative siblings. */
  "start int not null,"             /* Start of contig in BAC submission. */
  "size int not null,"              /* Size of contig. */
  "file_offset int not null,"       /* Offset of '>' for seq in FA file. */
  "file_size int not null,"         /* Size of seq in file. */
	       /* Extra indices. */
  "index (bac))";

char orderingTable[] =
/* Keeps track of orientation and offset of contigs. */
"create table ordering ("
  "id int not null primary key,"   /* Unique id. */
  "parent int not null,"           /* Parent in same table. */
  "piece smallint not null,"       /* Order relative to siblings. */
  "orientation tinyint not null,"  /* Orientation relative to parent. */
  "offset int not null,"           /* Offset within parent. */
  "length int not null,"           /* Length in base pairs. */
  "contig int not null,"           /* Contig, only at lowest level. */
	       /* Extra indices. */
  "index (contig),"
  "index (parent))";

/* The following defines help manage an inheritence tree of alignment
 * table structures. */

#ifdef OLDWAY
char cdnaAliTable[] =
/* Keeps track of a cdna/genomic alignment. */
"CREATE TABLE cdna_ali ("
  "id int not null primary key,"    /* Identity. */
  "query int not null,"             /* Query (seq) id. */
  "qstart int not null,"            /* Start offset into query. */
  "qend int not null,"              /* One past end offset into query. */
  "qorientation tinyint not null,"  /* Orientation relative to query. */
  "target int not null,"            /* Target (ordering) id. */
  "tstart int not null,"            /* Start offset into target. */
  "tend int not null,"              /* One past end offset into target. */
  "torientation tinyint not null,"  /* Orientation relative to target. */
  "score int not null,"             /* Overall log-odds score. */
  "intron_orientation tinyint not null," /* 1 for GT/AG introns. -1 for CT/AC,0 for none .*/
	       /* Extra indices. */
  "index (query),"
  "index (target),"
  "index (intron_orientation))";

char cdnaBlockTable[] = 
/* Keeps track of the blocks of a cdna/genomic alignment. */
"CREATE TABLE cdna_block ("
  "id int not null primary key,"    /* Identity. */
  "parent int not null,"            /* Parent ali. */
  "piece smallint not null,"        /* Piece within ali. */
  "query int not null,"             /* Query (seq) id. */
  "qstart int not null,"            /* Start offset into query. */
  "qend int not null,"              /* One past end offset into query. */
  "qorientation tinyint not null,"  /* Orientation relative to query. */
  "qinserts blob not null,"         /* Encoded list of query inserts. */
  "target int not null,"            /* Target (ordering) id. */
  "tstart int not null,"            /* Start offset into target. */
  "tend int not null,"              /* One past end offset into target. */
  "torientation tinyint not null,"  /* Orientation relative to target. */
  "tinserts blob not null,"         /* Encoded list of target inserts. */
  "score int not null,"             /* Log-odds score. */
	       /* Extra indices. */
  "index (target),"
  "index (parent))";

char blastxAliTable[] =
/* Keeps track of a cdna/genomic alignment. */
"CREATE TABLE blastx_ali ("
  "id int not null primary key,"    /* Identity. */
  "query_acc char(12) not null,"    /* Query accession number. */
  "qstart int not null,"            /* Start offset into query. */
  "qend int not null,"              /* One past end offset into query. */
  "target int not null,"            /* Target (ordering) id. */
  "tstart int not null,"            /* Start offset into target. */
  "tend int not null,"              /* One past end offset into target. */
  "torientation tinyint not null,"  /* Orientation relative to target. */
  "score int not null,"             /* Overall log-odds (bit) score. */
	       /* Extra indices. */
  "index (query_acc),"
  "index (target))";

char blastxBlockTable[] =
/* Keeps track of the blocks of an blastx alignment. */
"CREATE TABLE blastx_block ("
  "id int not null primary key,"    /* Identity. */
  "parent int not null,"            /* Parent ali. */
  "piece smallint not null,"        /* Piece within ali. */
  "query_acc char(12) not null,"    /* Query accession number. */
  "qstart int not null,"            /* Start offset into query. */
  "qend int not null,"              /* One past end offset into query. */
  "target int not null,"            /* Target (ordering) id. */
  "tstart int not null,"            /* Start offset into target. */
  "tend int not null,"              /* One past end offset into target. */
  "torientation tinyint not null,"  /* Orientation relative to target. */
  "score int not null,"             /* Log-odds (bit) score. */
  "tframe tinyint not null,"        /* Frame in target. */
  "iscore tinyint not null,"        /* Identity (0-100%) */
  "pscore tinyint not null,"        /* Positive match (0-100%) */
	       /* Extra indices. */
  "index (target),"
  "index (parent))";

char blastx14AliTable[] =
/* Keeps track of a cdna/genomic alignment. */
"CREATE TABLE blastx14_ali ("
  "id int not null primary key,"    /* Identity. */
  "query_acc char(12) not null,"         /* Query accession number. */
  "qstart int not null,"            /* Start offset into query. */
  "qend int not null,"              /* One past end offset into query. */
  "target int not null,"            /* Target (ordering) id. */
  "tstart int not null,"            /* Start offset into target. */
  "tend int not null,"              /* One past end offset into target. */
  "torientation tinyint not null,"  /* Orientation relative to target. */
  "score int not null,"             /* Overall log-odds (bit) score. */
	       /* Extra indices. */
  "index (query_acc),"
  "index (target))";

char blastx14BlockTable[] =
/* Keeps track of the blocks of an blastx alignment. */
"CREATE TABLE blastx14_block ("
  "id int not null primary key,"    /* Identity. */
  "parent int not null,"            /* Parent ali. */
  "piece smallint not null,"        /* Piece within ali. */
  "query_acc char(12) not null,"         /* Query accession number. */
  "qstart int not null,"            /* Start offset into query. */
  "qend int not null,"              /* One past end offset into query. */
  "target int not null,"            /* Target (ordering) id. */
  "tstart int not null,"            /* Start offset into target. */
  "tend int not null,"              /* One past end offset into target. */
  "torientation tinyint not null,"  /* Orientation relative to target. */
  "score int not null,"             /* Log-odds (bit) score. */
  "tframe tinyint not null,"        /* Frame in target. */
  "iscore tinyint not null,"        /* Identity (0-100%) */
  "pscore tinyint not null,"        /* Positive match (0-100%) */
	       /* Extra indices. */
  "index (target),"
  "index (parent))";
#endif /* OLDWAY */

char *uniqueTableNames[] =
    {
    "development", "cell", "cds", "geneName", "productName",
    "source", "organism", "library", "mrnaClone", "sex", "tissue",
    "center", "bacClone", "cytoMap", "chromosome",
    };

void createUniqTable(struct sqlConnection *conn, char *tableName)
/* Create a simple ID/Name pair table. */
{
char query[256];
sprintf(query,
    "create table %s (" 
       "id int not null primary key,"
       "name varchar(255) )",
    tableName); 
sqlUpdate(conn, query);
}


void removeTrailingSlash(char *dirName)
/* Remove trailing slash if any from dirName */
{
int len = strlen(dirName);
if (len > 0)
    {
    --len;
    if (dirName[len] == '/' || dirName[len] == '\\')
	dirName[len] = 0;
    }
}


struct uniqueTable
/* Help manage a table that is simply unique. */
    {
    struct uniqueTable *next;
    char *tableName;
    char *raField;
    struct hash *hash;
    HGID curId;
    FILE *tabFile;
    };

struct uniqueTable *uniqueTableList = NULL;

void clearUniqueIds()
/* Clear curId from all unique table entries. */
{
struct uniqueTable *uni;
for (uni = uniqueTableList; uni != NULL; uni = uni->next)
    uni->curId = 0;
}

void storeUniqueTables(struct sqlConnection *conn)
/* Close tab files, load tab files, free hashs from unique tables. */
{
struct uniqueTable *uni;
char *table;

for (uni = uniqueTableList; uni != NULL; uni = uni->next)
    {
    carefulClose(&uni->tabFile);
    table = uni->tableName;
    hgLoadTabFile(conn, table);
    freeHash(&uni->hash);
    }
}

struct uniqueTable *getUniqueTable(struct sqlConnection *conn, char *tableName, char *raField)
/* Return a new unique table.  Create it in database if it doesn't exist.  Load
 * up hash table with current values. */
{
struct uniqueTable *uni;
struct sqlResult *sr;
char **row;
char query[256];
struct hash *hash;
struct hashEl *hel;
int count = 0;

AllocVar(uni);
uni->tableName = tableName;
uni->hash = hash = newHash(0);
uni->raField = raField;
sprintf(query, "select id,name from %s", tableName);
sr = sqlGetResult(conn,query);
if (sr != NULL)
    {
    printf("Loading old %s values\n", tableName);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	HGID id = sqlUnsigned(row[0]);
	char *name = row[1];
	hel = hashAdd(hash, name, nullPt + id);
	++count;
	}
    }
sqlFreeResult(&sr);
slAddHead(&uniqueTableList, uni);
uni->tabFile = hgCreateTabFile(tableName);
return uni;
}

HGID uniqueStore(struct sqlConnection *conn, struct uniqueTable *uni, char *name)
/* Store name in unique table.  Return id associated with name. */
{
struct hash *hash = uni->hash;
struct hashEl *hel = hashLookup(hash, name);

if (hel != NULL)
    {
    return (char *)(hel->val) - nullPt;
    }
else
    {
    HGID id = hgNextId();
    hashAdd(hash, name, nullPt + id);
    fprintf(uni->tabFile, "%lu\t%s\n", id, name);
    return id;
    }
}

struct extFile
/* This stores info on an external file. */
    {
    HGID id;
    char *name;
    char *path;
    unsigned long size;
    };

struct hash *extFilesHash = NULL;

void loadExtFilesTable(struct sqlConnection *conn)
/* Convert external file table into extFilesHash. */
{
struct sqlResult *sr;
struct extFile *ex;
struct hash *hash;
struct hashEl *hel;
char **row;
char *name;
char lastChar;
int len;

if (extFilesHash == NULL)
    {
    extFilesHash = hash = newHash(8);
    sr = sqlGetResult(conn,"select id,name,path,size from extFile");
    while ((row = sqlNextRow(sr)) != NULL)
	{
	AllocVar(ex);
	ex->id = sqlUnsigned(row[0]);
	hel = hashAdd(hash, row[1], ex);
	ex->name = name = hel->name;
	ex->path = cloneString(row[2]);
	ex->size = sqlUnsigned(row[3]);
	len = strlen(name);
	lastChar = name[len-1];
	if (lastChar != '/' && ex->size != fileSize(ex->path))
	    {
	    errAbort("External file %s out of sync", ex->path);
	    }
	}
    sqlFreeResult(&sr);
    }
}

HGID storeExtFilesTable(struct sqlConnection *conn, char *name, char *path)
/* Store name/path in external files table. */
{
struct hashEl *hel;
struct extFile *ex;

loadExtFilesTable(conn);
if ((hel = hashLookup(extFilesHash, name)) != NULL)
    {
    ex = hel->val;
    if (!sameString(ex->path, path))
	errAbort("external file %s path mismatch\n%s vs. %s\n", name, path, ex->path);
    return ex->id;
    }
else
    {
    char query[512];
    HGID id;
    unsigned long size = fileSize(path);
    AllocVar(ex);
    ex->id = id = hgNextId();
    hel = hashAdd(extFilesHash, name, ex);
    ex->name = hel->name;
    ex->path = cloneString(path);
    ex->size = size;
    sprintf(query, "INSERT into extFile VALUES(%lu,'%s','%s',%lu)",
	id, name, path, size);
    sqlUpdate(conn,query);
    return id;
    }
}

boolean faSeekNextRecord(struct lineFile *faLf)
/* Seeks to the next FA record.  Returns FALSE if seeks to EOF. */
{
char *faLine;
int faLineSize;
while (lineFileNext(faLf, &faLine, &faLineSize))
    {
    if (faLine[0] == '>')
	return TRUE;
    }
return FALSE;
}

void gbDateToSqlDate(char *gbDate, char *sqlDate)
/* Convert genbank to SQL date. */
{
static char *months[] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN", 
                          "JUL", "AUG", "SEP", "OCT", "NOV", "DEC", };
int monthIx;
char *day = gbDate;
char *month = gbDate+3;
char *year = gbDate+7;

gbDate[2] = gbDate[6] = 0;
for (monthIx = 0; monthIx<12; ++monthIx)
    {
    if (sameString(months[monthIx], month))
	break;
    }
if (monthIx == 12)
    errAbort("Unrecognized month %s", month);
sprintf(sqlDate, "%s-%02d-%s", year, monthIx, day);
}

char *bacFileName(char *dir, char *acc, char nameBuf[512])
/* Return file name bac is in. */
{
sprintf(nameBuf, "%s%s.fa", dir, acc);
return nameBuf;
}

struct filePos
/* Little helper struct to help us analyse fa files. */
    {
    struct filePos *next;   /* Next in list. */
    long offset;            /* Start offset. */
    int size;               /* Size of record. */
    };

void faParseRecords(char *fileName, struct filePos **retPos, long *retSize)
/* Figure out record positions in FA file. */
{
struct lineFile *lf = lineFileOpen(fileName, FALSE);
struct filePos *posList = NULL, *pos;
long start, end = 0;
char *line;
int lineSize;
boolean moreLeft;

if (!lineFileNext(lf, &line, &lineSize) || line[0] != '>')
    errAbort("%s doesn't appear to be an fa file.", fileName);
for (;;)
    {
    start = lf->bufOffsetInFile + lf->lineStart;
    for (;;)
	{
	moreLeft = lineFileNext(lf, &line, &lineSize);
	if (!moreLeft || line[0] == '>')
	    {
	    end = lf->bufOffsetInFile + lf->lineStart;
	    AllocVar(pos);
	    pos->offset = start;
	    pos->size = end-start;
	    slAddHead(&posList, pos);
	    break;
	    }
	}
    if (!moreLeft)
	break;
    }
lineFileClose(&lf);
slReverse(&posList);
*retPos =  posList;
*retSize = end;
}

void addBac(char *symName, char *faDir, char *raName)
/* Add in BAC data from fa and ra files. */
{
struct sqlConnection *conn = hgStartUpdate();
struct hash *raFieldHash = newHash(8);
struct lineFile *raLf;
char raAcc[32];
char nameBuf[512];
long extFileId = storeExtFilesTable(conn, symName, faDir);
char *raTag;
char *raVal;
int raLineSize;
int maxMod = 20;
int mod = maxMod;
int lineMaxMod = 50;
int lineMod = lineMaxMod;
int count = 0;
FILE *contigTab = hgCreateTabFile("contig");
FILE *orderingTab = hgCreateTabFile("ordering");
FILE *bacTab = hgCreateTabFile("bac");
FILE *seqTab = hgCreateTabFile("seq");
struct uniqueTable *uniSrc, *uniOrg, *uniLib, *uniClo, *uniSex, *uniCen, *uniMap, *uniChr;
int contigPad = 800;

hashAdd(raFieldHash, "src", uniSrc = getUniqueTable(conn, "source", "src"));
hashAdd(raFieldHash, "org", uniOrg = getUniqueTable(conn, "organism", "org"));
hashAdd(raFieldHash, "lib", uniLib = getUniqueTable(conn, "library", "lib"));
hashAdd(raFieldHash, "clo", uniClo = getUniqueTable(conn, "bacClone", "clo"));
hashAdd(raFieldHash, "sex", uniSex = getUniqueTable(conn, "sex", "sex"));
hashAdd(raFieldHash, "cen", uniCen = getUniqueTable(conn, "center", "cen"));
hashAdd(raFieldHash, "map", uniMap = getUniqueTable(conn, "cytoMap", "map"));
hashAdd(raFieldHash, "chr", uniChr = getUniqueTable(conn, "chromosome", "chr"));

/* Open input files. */
uglyf("opening %s\n", raName);
raLf = lineFileOpen(raName, TRUE);

/* Loop around for each record of RA */
for (;;)
    {
    boolean gotRaAcc = FALSE, gotRaDate = FALSE;
    boolean gotAny = FALSE;
    HGID bacId;
    HGID bacOrderingId;
    long faSize;
    boolean atEof;
    int len;
    char *s;
    char sqlDate[32];
    char *contigString = NULL;
    int version = 0;
    int phase = 3;
    int frags = 1;
    int dnaSize = 0;
    int contigCount = 1;
    int orderOffset = 0;
    struct filePos *faPosList = NULL, *pos;
    int faContigCount;

    /* Print out I'm alive dots. */
    ++count;
    if (--mod == 0)
	{
	printf(".");
	fflush(stdout);
	mod = maxMod;
	if (--lineMod == 0)
	    {
	    printf("%d\n", count);
	    lineMod = lineMaxMod;
	    }
	}

    /* Get next RA record. */
    clearUniqueIds();
    bacId = hgNextId();
    bacOrderingId = hgNextId();
    for (;;)
	{
	struct hashEl *hel;

	if (!lineFileNext(raLf, &raTag, &raLineSize))
	    break;
	gotAny = TRUE;
	if (raTag[0] == 0)
	    break;
	raVal = strchr(raTag, ' ');
	if (raVal == NULL)
	    errAbort("Badly formatted tag line %d of %s", raLf->lineIx, raLf->fileName);
	*raVal++ = 0;
	if ((hel = hashLookup(raFieldHash, raTag)) != NULL)
	    {
	    struct uniqueTable *uni = hel->val;
	    uni->curId = uniqueStore(conn,uni,raVal);
	    }
	else if (sameString(raTag, "acc"))
	    {
	    gotRaAcc = TRUE;
	    s = firstWordInLine(raVal);
	    len = strlen(s);
	    if (len >= sizeof(raAcc))
		errAbort("Accession too long line %d of %s", raLf->lineIx, raLf->fileName);
	    strcpy(raAcc, s);
	    }
	else if (sameString(raTag, "ver"))
	    {
	    version = sqlUnsigned(raTag);
	    }
	else if (sameString(raTag, "dat"))
	    {
	    gbDateToSqlDate(raVal, sqlDate);
	    gotRaDate = TRUE;
	    }
	else if (sameString(raTag, "frg"))
	    {
	    frags = sqlUnsigned(raVal);
	    }
	else if (sameString(raTag, "pha"))
	    {
	    phase = sqlUnsigned(raVal);
	    }
	else if (sameString(raTag, "siz"))
	    {
	    dnaSize = sqlUnsigned(raVal);
	    }
	else if (sameString(raTag, "ctg"))
	    {
	    contigString = cloneString(raVal);
	    }
	}
    if (!gotAny)
	break;

    /* Do a little error checking and then write out to tables. */
    if (!gotRaAcc)
	errAbort("No accession in %s\n", raName);
    if (!gotRaDate)
	errAbort("No date in %s\n", raAcc);
    if (!dnaSize)
	errAbort("No size in %s\n", raAcc);

    /* Get next FA file info. */
    bacFileName(faDir, raAcc, nameBuf);
    faParseRecords(nameBuf,  &faPosList, &faSize);
    faContigCount = slCount(faPosList);

    if (contigString != NULL)
	{
	static char *words[512];
	char *s, *e;
	int i;

	contigCount = chopLine(contigString, words);
	if (contigCount != faContigCount)
	    errAbort("contig mismatch between fa and ra in %s", raAcc);
	for (i=0,pos=faPosList; i<contigCount; ++i,pos=pos->next)
	    {
	    int start, end, size;
	    HGID orderingId = hgNextId();
	    HGID contigId = hgNextId();
	    s = words[i];
	    e = strchr(s, '-');
	    *e++ = 0;
	    start = sqlUnsigned(s);
	    end = sqlUnsigned(e);
	    size = end-start;
	    fprintf(contigTab, "%lu\t%lu\t%d\t%d\t%d\t%ld\t%d\n", 
		 contigId, bacId, i, start, size, pos->offset, pos->size);
	    fprintf(orderingTab, "%lu\t%lu\t%d\t%d\t%d\t%d\t%lu\n",
		    orderingId, bacOrderingId, i, 1, orderOffset, size, contigId);
	    orderOffset += contigPad + size;
	    }
	fprintf(orderingTab, "%lu\t%lu\t%d\t%d\t%d\t%d\t%lu\n",
		bacOrderingId, 0L, 0, 1, 0, orderOffset-contigPad, 0L);
	freez(&contigString);
	}
    /* If no contig then make one up out of size. */
    else 
	{
	HGID contigId = hgNextId();
	if (faContigCount != 1)
	    errAbort("contig mismatch between fa and ra in %s", raAcc);
	pos = faPosList;
	fprintf(contigTab, "%lu\t%lu\t%d\t%d\t%ld\t%d\n", 
		contigId, bacId, 0, dnaSize, pos->offset, pos->size);
	fprintf(orderingTab, "%lu\t%lu\t%d\t%d\t%d\t%d\t%lu\n",
		bacOrderingId, 0L, 0, 1, 0, dnaSize, contigId);
	}

    fprintf(seqTab, "%lu\t%s\t%d\t%s\t%lu\t%lu\t%d\n",
	bacId, raAcc, dnaSize, sqlDate, extFileId, 0L, faSize);
    fprintf(bacTab, "%u\t%s\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\n",
	bacId, raAcc, version,
	uniSrc->curId, uniOrg->curId, uniLib->curId, uniClo->curId, uniSex->curId, 
	phase, uniCen->curId, frags, uniMap->curId, uniChr->curId, bacOrderingId);

    slFreeList(&faPosList);
    }
printf("%d\n", count);
storeUniqueTables(conn);
lineFileClose(&raLf);
fclose(orderingTab);
fclose(contigTab);
fclose(bacTab);
fclose(seqTab);
hgLoadTabFile(conn, "ordering");
hgLoadTabFile(conn, "contig");
hgLoadTabFile(conn, "bac");
hgLoadTabFile(conn, "seq");
hgEndUpdate(&conn, "Add BAC directory %s", faDir);
}

void notSfFile(struct lineFile *lf)
/* Complain that file isn't correct sort-filtered alignment format. */
{
if (lf == NULL)
    errAbort("Bad .sf file.");
else
    errAbort("%s doesn't seem to be a good .sf file.  Error line %d", 
	    lf->fileName, lf->lineIx);
}

void parseContig(char *contig, char retAcc[16], int *retIx)
/* Parse contig into accession and contig Ix parts.  */
{
char *s = strchr(contig, '.');
int accSize;
if (s == NULL)
    errAbort("No .ix in contig %s", contig);
accSize = s - contig;
if (accSize >= 16)
    errAbort("Accession too long in contig %s", contig);
memcpy(retAcc, contig, accSize);
retAcc[accSize] = 0;
*retIx = sqlUnsigned(s+1);
}

struct sfLine
/* This holds a parsed version of a line of an sf file. */
    {
    struct sfLine *next;	/* Next in list. */
    double psuedoPercent;       /* psuedoPercent score 0-100. */
    char strand;                /* + or - */
    char query[32];             /* Query sequence name. */
    int qStart, qEnd;           /* Start/end of alignment in query. */
    int qSize;                  /* Total size of query. */
    char target[32];            /* Target sequence name. */
    int tStart, tEnd;           /* Start/end of alignment in target. */
    int tSize;                  /* Total size of target. */
    };

void parseSfLine(struct lineFile *lf, char *line,  struct sfLine *ret)
/* Parse line and return values in ret.  Lf parameter just used for
 * error reporting.  Returns FALSE if not a real sf line. */
{
char *words[16];
int wordCount;
char *parts[4];
int partCount;
double psuedoPercent;


wordCount = chopLine(line, words);
if (wordCount != 9)
    notSfFile(lf);
ret->psuedoPercent = psuedoPercent  = atof(words[0]);
if (psuedoPercent <= 0 || psuedoPercent > 100)
    notSfFile(lf);
ret->strand = words[1][0];
if ((partCount = chopString(words[2], ":-", parts, ArraySize(parts))) != 3)
    notSfFile(lf);
strncpy(ret->query, parts[0], sizeof(ret->query));
ret->qStart = sqlUnsigned(parts[1]);
ret->qEnd = sqlUnsigned(parts[2]);
ret->qSize = sqlUnsigned(words[4]);
if ((partCount = chopString(words[6], ":-", parts, ArraySize(parts))) != 3)
    notSfFile(lf);
strncpy(ret->target, parts[0], sizeof(ret->target));
ret->tStart = sqlUnsigned(parts[1]);
ret->tEnd = sqlUnsigned(parts[2]);
ret->tSize = sqlUnsigned(words[8]);
}

int ffAlisInBlock(struct ffAli *ali)
/* Return number of alis that are separated by small enough gaps
 * that they should be stored in same block. */
{
enum {maxInsert = 20,};
int count = 1;
struct ffAli *right;

for (;;)
    {
    if ((right = ali->right) == NULL)
	break;
    if (right->nStart - ali->nEnd > maxInsert)
	break;
    if (right->hStart - ali->hEnd > maxInsert)
	break;
    ++count;
    ali = right;
    }
return count;
}

struct ffAli *advanceFfAli(struct ffAli *ali, int count)
/* Return ali count ali's to the right. */
{
while (--count >= 0)
    ali = ali->right;
return ali;
}

int intronOrientation(struct ffAli *left, struct ffAli *right)
/* Return 1 for GT/AG intron between left and right, -1 for CT/AC, 0 for no
 * intron. */
{
if (left->nEnd == right->nStart && right->hStart - left->hEnd > 40)
    {
    DNA *iStart = left->hEnd;
    DNA *iEnd = right->hStart;
    if (iStart[0] == 'g' && iStart[1] == 't' && iEnd[-2] == 'a' && iEnd[-1] == 'g')
	{
	return 1;
	}
    else if (iStart[0] == 'c' && iStart[1] == 't' && iEnd[-2] == 'a' && iEnd[-1] == 'c')
	{
	return -1;
	}
    else
	return 0;
    }
else
    return 0;
}

int gapPenalty(struct ffAli *left, struct ffAli *right)
/* Return gap penalty. */
{
int nGap, hGap, gap;
nGap = right->nStart - left->nEnd;
if (nGap < 0)
    nGap = -nGap;
hGap = right->hStart - left->hEnd;
if (hGap < 0)
    hGap = -hGap;
gap = max(nGap,hGap);
if (gap < 2)
    return 8;
else if (gap < 3)
    return 12;
else if (gap < 5)
    return 13;
else if (gap < 8)
    return 14;
else
    return 15;
}

void dumpFf(struct ffAli *ff, DNA *needle, DNA *haystack)
/* Print out a ffAli. */
{
printf("Dumping ffAli\n");
while (ff != NULL)
    {
    printf("n %d-%d h %d-%d\n", ff->nStart-needle, ff->nEnd-needle,
	ff->hStart-haystack, ff->hEnd-haystack);
    ff = ff->right;
    }
}

void addCdnaAli(char *sfFile)
/* Add in alignments from a sortFilt file. */
{
struct lineFile *lf = lineFileOpen(sfFile, TRUE);
struct sqlConnection *conn = hgStartUpdate();
char *line;
int lineSize;
char tAcc[16];
int tContigIx;
struct sfLine sf;
struct dnaSeq *query, *target;
DNA *qDna, *tDna;
struct hgContig *contig;
struct ffAli *ff;
DNA *qAliStart, *tAliStart;
struct dyString *qInserts = newDyString(128), *tInserts = newDyString(128);
char nameBuf[256];
HGID queryId;
HGID aliId, blockId;
int countInBlock;
int i;
FILE *cdnaAliTab = hgCreateTabFile("cdna_ali");
FILE *cdnaBlockTab = hgCreateTabFile("cdna_block");
int maxMod = 200;
int mod = maxMod;
int maxLineMod = 50;
int lineMod = maxLineMod;

while (lineFileNext(lf, &line, &lineSize))
    {
    int curOrientation = 0, newOrientation;
    int blockIx;
    int aliScore = 0;
    struct ffAli *left,*right,*blockStart,*blockEnd,*ffEnd;

    if (--mod == 0)
	{
	printf(".");
	fflush(stdout);
	mod = maxMod;
	if (--lineMod == 0)
	    {
	    printf("%d\n", lf->lineIx);
	    lineMod = maxLineMod;
	    }
	}
    parseSfLine(lf, line, &sf);
    parseContig(sf.target, tAcc, &tContigIx);
    tContigIx -= 1;	/* Adjust between 1 to N  and 0 to N-1 coordinates. */
    hgRnaSeqAndId(sf.query, &query, &queryId);
    contig = hgGetContig(tAcc, tContigIx);
    target = hgContigSeq(contig);
    qDna = query->dna;
    tDna = target->dna;
    qAliStart = qDna + sf.qStart;
    tAliStart = tDna + sf.tStart;
    if (sf.strand == '-')
	reverseComplement(qDna, query->size);
    ff = ffFind(qAliStart, qDna + sf.qEnd,
    	       tAliStart, tDna + sf.tEnd, ffCdna);
    if (ff == NULL)
	{
	errAbort("Couldn't realign %s:%d-%d strand %c to %s:%d-%d",
	    sf.query, sf.qStart, sf.qEnd, sf.strand,
	    sf.target, sf.tStart, sf.tEnd);
	}
    left = ff;
    aliId = hgNextId();
    for (blockIx = 0; ; ++blockIx)
	{
	int qGapSize, tGapSize;
	int blockScore;
	blockStart = left;
	dyStringClear(qInserts);
	dyStringClear(tInserts);
	countInBlock = ffAlisInBlock(blockStart);
	for (i=1; i<countInBlock; ++i)
	    {
	    DNA *qGapStart, *tGapStart;

	    right = left->right;
	    qGapStart = left->nEnd;
	    qGapSize = right->nStart - qGapStart;
	    if (qGapSize > 0)
		{
		int gapStart = qGapStart - qAliStart;
		if (qGapSize == 1)
		    dyStringPrintf(qInserts, "%d,", gapStart);
		else
		    dyStringPrintf(qInserts, "%dx%d,", gapStart, qGapSize);
		}
	    tGapStart = left->hEnd;
	    tGapSize = right->hStart - tGapStart;
	    if (tGapSize > 0)
		{
		int gapStart = tGapStart - tAliStart;
		if (tGapSize == 1)
		    dyStringPrintf(tInserts, "%d,", gapStart);
		else
		    dyStringPrintf(tInserts, "%dx%d,", gapStart, tGapSize);
		}
	    left = right;
	    }
	blockEnd = left;
	blockScore = ffScoreSomeAlis(blockStart, countInBlock, ffCdna);
	aliScore += blockScore;
	fprintf(cdnaBlockTab, 
	    "%lu\t%lu\t%d\t%lu\t%d\t%d\t%d\t%s\t%lu\t%d\t%d\t%d\t%s\t%d\n",
	    hgNextId(), aliId, blockIx, queryId, 
	    blockStart->nStart - qDna, blockEnd->nEnd - qDna,
	    (sf.strand == '-' ? -1 : 1), qInserts->string,
	    contig->nest->id, 
	    blockStart->hStart - tDna, blockEnd->hEnd - tDna,
	    1, tInserts->string,
	    blockScore );
	right = left->right;
	if (right == NULL)
	    break;
	newOrientation = intronOrientation(left, right);
	if (newOrientation != 0)
	    {
	    if (curOrientation == 0)
		curOrientation = newOrientation;
	    else if (curOrientation != newOrientation)
		curOrientation = 0;	/* Mixed signals cancel. */
	    }
	else
	    {
	    aliScore -= ffCdnaGapPenalty(left,right); /* Charge for non-intron gap. */
	    }
	left = right;
	}
    ffEnd = left;
    fprintf(cdnaAliTab,
	"%lu\t%lu\t%d\t%d\t%d\t%lu\t%d\t%d\t%d\t%d\t%d\n",
	aliId, queryId,  sf.qStart, sf.qEnd, 
	(sf.strand == '-' ? -1 : 1), 
	contig->nest->id, sf.tStart, sf.tEnd, 1,
	aliScore, curOrientation);

    ffFreeAli(&ff);
    freeDnaSeq(&target);
    freeDnaSeq(&query);
    }
printf("%d\n", lf->lineIx);
lineFileClose(&lf);
freeDyString(&qInserts);
freeDyString(&tInserts);
fclose(cdnaAliTab);
fclose(cdnaBlockTab);
hgLoadTabFile(conn, "cdna_ali");
hgLoadTabFile(conn, "cdna_block");
hgEndUpdate(&conn, "cDNA hits from %s", sfFile);
}

char *scanToLine(struct lineFile *lf, char *lineStart)
/* Scan in lineFile until get line that starts with lineStart.
 * Return NULL at eof. */
{
char *line;
int lineSize;

while (lineFileNext(lf, &line, &lineSize))
    {
    if (startsWith(lineStart, line))
	return line;
    }
return NULL;
}

char *scanPastWhiteToLine(struct lineFile *lf, char *lineStart)
/* Scan in lineFile until get line that starts with lineStart
 * after whitespace Return NULL at eof. */
{
char *line;
int lineSize;

while (lineFileNext(lf, &line, &lineSize))
    {
    if (startsWith(lineStart, skipLeadingSpaces(line)))
	return line;
    }
return NULL;
}

boolean isAcc(char *s)
/* Returns TRUE if s looks to be a GenBank accession. */
{
char c;
if (!isupper(s[0])) return FALSE;
if (!isdigit(s[1]) && !isupper(s[1])) return FALSE;
s += 2;
while ((c = *s++) != 0)
    if (!isdigit(c)) return FALSE;
return TRUE;
}

void blastxParseError(struct lineFile *lf)
/* Complain about blastx file and abort */
{
errAbort("blastx parsing error line %d of %s", lf->lineIx, lf->fileName);
}

void findAcc(struct lineFile *lf, char *line, char acc[17])
/* Find accession in parenthesis or failing that after >gi. */
{
int lineSize;
char gAcc[32];
boolean gotGacc = FALSE;

if (startsWith(">gi|", line))
    {
    char *s = line+4;
    char *e = strchr(s, '|');
    if (e == NULL)
	e = strchr(s, ' ');
    if (e != NULL && e - s < sizeof(gAcc)-1 && isdigit(s[0]) && isdigit(e[-1]))
	{
	int numSize = e - s;
	gAcc[0] = 'g';
	memcpy(gAcc+1, s, numSize);
	gAcc[numSize+1] = 0;
	gotGacc = TRUE;
	}
    }

for (;;)
    {
    char *closeP;
    while ((line = strchr(line, '(')) != NULL)
	{
	++line;
	if ((closeP = strchr(line, ')')) != NULL)
	    {
	    int size = closeP - line;
	    if (size >= 6 && size <= 14)
		{
		memcpy(acc, line, size);
		acc[size] = 0;
		if (isAcc(acc))
		    return;
		}
	    }
	}
    if (!lineFileNext(lf, &line, &lineSize) || !isspace(line[0]))
	errAbort("Accession in parenthesis not found near line %d of %s", lf->lineIx, lf->fileName);
    line = skipLeadingSpaces(line);
    if (startsWith("Length", line))
	{
	if (gotGacc)
	    {
	    strcpy(acc, gAcc);
	    }
	else
	    {
	    errAbort("Can't find accession near line %d of %s", lf->lineIx, lf->fileName);
	    }
	lineFileReuse(lf);
	break;
	}
    }
}

struct blastxBlock
/* Memory copy of blastx aligment block. */
    {
    struct blastxBlock *next;	/* Next in list */
    HGID id;			/* Block id. */
    HGID parent;		/* Block of parent alignment. */
    int piece;                  /* Which piece of alignment. */
    char queryAcc[16];          /* Query (protein) accession. */
    int qStart, qEnd;		/* Position in query. */
    struct hgNest *target;      /* Target in nest tree. */
    int tStart, tEnd;           /* Target coordinates. */
    int tOrientation;           /* +1 or -1 strand. */
    int score;                  /* Log odds (bit) score. */
    int tFrame;                 /* Frame - 0, 1, or 2 */
    int iScore;                 /* Identity score 0-100 */
    int pScore;                 /* Positive match score 0-100 */
    };

int cmpBlastxBlockQstart(const void *va, const void *vb)
/* Compare two hg. */
{
const struct blastxBlock *a = *((struct blastxBlock **)va);
const struct blastxBlock *b = *((struct blastxBlock **)vb);
return a->qStart - b->qStart;
}

void addBlastxAli(char *cloneAcc, int contigIx, struct lineFile *lf, char *line,
	double maxE, FILE *aliTab, FILE *blockTab)
/* Parse through one blastx alignment and add to database. */
{
char proteinAcc[32];
int proteinSize;
char *words[64];
int wordCount;
int lineSize;
int accLineCount;
HGID parentId;
struct blastxBlock *blockList = NULL, *block;

accLineCount = lf->lineIx;
findAcc(lf, line, proteinAcc);
line = scanPastWhiteToLine(lf, "Length");
wordCount = chopLine(line, words);
if (wordCount != 3 || !isdigit(words[2][0]))
    blastxParseError(lf);
proteinSize = sqlUnsigned(words[2]);
lineFileNext(lf, &line, &lineSize);	/* Blank line. */


for (;;)
    {
    int bitScore, iScore, pScore, frame;
    char dir;
    double eScore;
    int qStart, sStart;
    int qEnd, sEnd;

    qStart = sStart = 0x3fffffff;
    qEnd = sEnd = -qStart;

    if (!lineFileNext(lf, &line, &lineSize))
	break;
    if (!startsWith(" Score =", line))
	{
	lineFileReuse(lf);
	break;
	}
    wordCount = chopLine(line, words);
    if (wordCount != 8 || !isdigit(words[2][0]))
	blastxParseError(lf);
    bitScore = atoi(words[2]);
    eScore = atof(words[7]);
    if (!lineFileNext(lf, &line, &lineSize))
	blastxParseError(lf);
    wordCount = chopLine(line, words);
    if (wordCount < 8 || words[3][0] != '(' || !sameString(words[4], "Positives") || words[7][0] != '(')
	blastxParseError(lf);
    iScore = atoi(words[3]+1);
    pScore = atoi(words[7]+1);
    if (!lineFileNext(lf, &line, &lineSize))
	blastxParseError(lf);
    wordCount = chopLine(line, words);
    if (wordCount != 3 || !sameString("Frame", words[0]))
	blastxParseError(lf);
    dir = words[2][0];
    frame = sqlUnsigned(words[2]+1);
    if (!lineFileNext(lf, &line, &lineSize))	/* Blank line. */
	blastxParseError(lf);

    for (;;)
    /* Loop through Query: ... Subject: quadruple lines. */
	{
	if (!lineFileNext(lf, &line, &lineSize))	
	    blastxParseError(lf);
	else if (startsWith("Query:", line))
	    {
	    int s, e;
	    wordCount = chopLine(line, words);
	    if (wordCount != 4)
		blastxParseError(lf);
	    if (dir == '+')
		{
		s = sqlUnsigned(words[1]);
		if (s < qStart)
		    qStart = s;
		e = sqlUnsigned(words[3]);
		if (e > qEnd)
		    qEnd = e;
		}
	    else
		{
		s = sqlUnsigned(words[3]);
		if (s < qStart)
		    qStart = s;
		e = sqlUnsigned(words[1]);
		if (e > qEnd)
		    qEnd = e;
		}
	    if (!lineFileNext(lf, &line, &lineSize))	/* Skip middle alignment line. */
		blastxParseError(lf);
	    if (!lineFileNext(lf, &line, &lineSize))	
		blastxParseError(lf);
	    if (!startsWith("Sbjct:", line))
		blastxParseError(lf);
	    wordCount = chopLine(line, words);
	    if (wordCount != 4)
		blastxParseError(lf);
	    s = sqlUnsigned(words[1]);
	    if (s < sStart)
		sStart = s;
	    e = sqlUnsigned(words[3]);
	    if (e > sEnd)
		sEnd = e;
	    if (!lineFileNext(lf, &line, &lineSize))	/* Blank line. */
		blastxParseError(lf);
	    }
	else if (skipLeadingSpaces(line)[0] == 0)	/* Ends with blank line. */
	    break;
	else
	    blastxParseError(lf);
	}
    qStart -= 1;	/* Convert to 0 to N-1 coordinates. */
    sStart -= 1;	/* Convert to 0 to N-1 coordinates. */
    if (eScore <= maxE)
	{
	AllocVar(block);
	strcpy(block->queryAcc, proteinAcc);
	block->qStart = sStart;		/* Query and Subject reversed in blasting... */
	block->qEnd = sEnd;
	block->target = hgGetContig(cloneAcc, contigIx)->nest;
	block->tStart = qStart;
	block->tEnd = qEnd;
	block->tOrientation = (dir == '-' ? -1 : 1);
	block->score = bitScore;
	block->tFrame = frame;
	block->iScore = iScore;
	block->pScore = pScore;
	slAddHead(&blockList, block);
	}
    }
if (blockList != NULL)
    {
    int pieceIx = 0;
    HGID parentId = hgNextId();
    int parentScore = 0;
    int qStart = blockList->qStart, tStart = blockList->tStart;   /* Parent start coordinates. */
    int qEnd = blockList->qEnd, tEnd = blockList->tEnd;       /* Parent end coordinates. */

    slSort(&blockList, cmpBlastxBlockQstart);
    for (block = blockList; block != NULL; block = block->next)
	{
	block->id = hgNextId();
	block->parent = parentId;
	block->piece = pieceIx++;
	}
    for (block = blockList; block != NULL; block = block->next)
	{
	fprintf(blockTab, "%lu\t%lu\t%d\t%s\t%d\t%d\t%lu\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
	    block->id, block->parent, block->piece, 
	    block->queryAcc, block->qStart, block->qEnd,
	    block->target->id,  block->tStart, block->tEnd, 
	    block->tOrientation, block->score, block->tFrame,
	    block->iScore, block->pScore);
	parentScore += block->score;
	if (block->tStart < tStart) tStart = block->tStart;
	if (block->tEnd > tEnd) tEnd = block->tEnd;
	if (block->qStart < qStart) qStart = block->qStart;
	if (block->qEnd > qEnd) qEnd = block->qEnd;
	}
    fprintf(aliTab, "%lu\t%s\t%d\t%d\t%lu\t%d\t%d\t%d\t%d\n",
	parentId, proteinAcc, qStart, qEnd, 
	blockList->target->id, tStart, tEnd,
	blockList->tOrientation, parentScore);
    }
slFreeList(&blockList);
}

void oneBlastxFile(char *xblastFile, double maxE, 
	FILE *ali_tab, FILE *block_tab)
/* Load an blastx file into database. */
{
char acc[32];
struct lineFile *lf = lineFileOpen(xblastFile, TRUE);
int lineSize;
char *line;
int contigIx = 0;

splitPath(xblastFile, NULL, acc, NULL);
if (!lineFileNext(lf, &line, &lineSize) || !startsWith("BLASTX", line))
    errAbort("%s doesn't appear to be an blastx output file", xblastFile);

while (scanToLine(lf, "Searching"))
    {
    boolean gotAny;
    for (;;)
	{
	if (!lineFileNext(lf, &line, &lineSize))
	   blastxParseError(lf); 
	if (startsWith("Sequences", line))
	    {
	    gotAny = TRUE;
	    break;
	    }
	if (startsWith(" ***** No hits", line))
	    {
	    gotAny = FALSE;
	    break;
	    }
	}
    if (gotAny)
	{
	scanToLine(lf, ">");
	lineFileReuse(lf);
	for (;;)
	    {
	    if (!lineFileNext(lf, &line, &lineSize))
		break;
	    if (line[0] == '>')
		{
		if (strstr(line, "ALU") != NULL && strstr(line,"WARNING") != NULL)	
		    {
		    /* ALU warning entry - skip. */
		    scanToLine(lf, ">");
		    lineFileReuse(lf);
		    }
		else
		    {
		    addBlastxAli(acc, contigIx, lf, line, maxE, ali_tab, block_tab);
		    }
		}
	    else if (startsWith("  Database:", line))
		break;
	    else if (skipLeadingSpaces(line)[0] != 0)
		errAbort("Unrecognized line %d of %s\n:%s\n", lf->lineIx, lf->fileName, line);
	    }
	}
    ++contigIx;
    }
}

#ifdef OLDWAY
struct blastxTableInfo
/* Info on a set of blastx tables (lets us have more than
 * one blastx table) */
    {
    char *blockName;
    char *aliName;
    char *blockCreate;
    char *aliCreate;
    };

struct blastxTableInfo btiDefault =
    {
    "blastx_block",
    "blastx_ali",
    blastxAliTable,
    blastxBlockTable,
    };
struct blastxTableInfo bti14 =
    {
    "blastx14_block",
    "blastx14_ali",
    blastx14AliTable,
    blastx14BlockTable,
    };


void addBlastxFileList(char *dir, struct slName *fileList, double maxE, struct blastxTableInfo *bti)
/* Add in a list of blastx files to database.  Dir should include trailing
 * / (or be empty). */
{
struct sqlConnection *conn = hgStartUpdate();
FILE *aliTab = hgCreateTabFile(bti->aliName);
FILE *blockTab = hgCreateTabFile(bti->blockName);
struct slName *name;
char path[512];

for (name = fileList; name != NULL; name = name->next)
    {
    sprintf(path, "%s%s", dir, name->name);
    oneBlastxFile(path, maxE, aliTab, blockTab);
    }
fclose(aliTab);
fclose(blockTab);
hgLoadTabFile(conn, bti->aliName);
hgLoadTabFile(conn, bti->blockName);
hgEndUpdate(&conn, "Add %d blastx files to %s and %s", slCount(fileList), bti->aliName, bti->blockName);
}

void addBlastxDir(char *dirName, char *suffix, struct blastxTableInfo *bti)
/* Add in blastx from dir matching suffix. */
{
char pattern[64];
char fullDir[512];
struct slName *fileList;

if (strlen(suffix) >= sizeof(pattern)-2)
    errAbort("Suffix too long, sorry.");
sprintf(pattern, "*%s", suffix);
removeTrailingSlash(dirName);
fileList = listDir(dirName, pattern);
sprintf(fullDir, "%s/", dirName);
addBlastxFileList(fullDir, fileList, 0.0001, bti);
slFreeList(&fileList);
}


struct slName *slNamesFromArray(char **array, int size)
/* Return a name list made up from array. */
{
int i;
struct slName *list = NULL, *el;

for (i=0; i<size; ++i)
    {
    el = newSlName(array[i]);
    slAddHead(&list, el);
    }
slReverse(&list);
return list;
}

void addBlastxFiles(char **files, int fileCount, struct blastxTableInfo *bti)
/* Add in blastx files. */
{
struct slName *fileList = slNamesFromArray(files, fileCount);
addBlastxFileList("", fileList, 0.0001, bti);
slFreeList(&fileList);
}

#endif /* OLDWAY */

void dropTable(struct sqlConnection *conn, char *tableName)
/* Drop named table. */
{
char query[128];
sprintf(query, "drop table %s", tableName);
sqlUpdate(conn,query);
}

void getAccFromPath(char *pathName, char accBuf[32])
/* Get accession from path name */
{
char *s = strrchr(pathName, '/');
char *e;
int len;

if (s == NULL)
    s = pathName;
else
    s += 1;
e = strrchr(s, '.');
if (e == NULL)
    e = s + strlen(s);
len = e - s;
if (len >= 32)
    errAbort("Accession too long in %s\n", pathName);
memcpy(accBuf, s, len);
accBuf[len] = 0;
}

void updateBac(char *symName, char *faDir)
/* Update bac, seq, and contig tables to refer to new bac's.
 * Currently an error for these not to exist in old database. */
{
struct slName *faNameList, *faName;
struct dnaSeq *contigList, *contig;
HGID extFileId;
int bacId;
char nameBuf[512];
char acc[32];
char query[256];
struct sqlConnection *conn = hgStartUpdate();
struct sqlConnection *delConn = sqlConnect(database);
struct sqlResult *sr;
struct sqlResult *delRes;
char **row;

/* Remove optional trailing slash if there.  Then construct
 * something that we know has slash for external file table. */
removeTrailingSlash(faDir);
sprintf(nameBuf, "%s/", faDir);
extFileId = storeExtFilesTable(conn, symName, nameBuf);

/* Get list of all fa files in dir and loop through it. */
faNameList = listDir(faDir, "*.fa");
for (faName = faNameList; faName != NULL; faName = faName->next)
    {
    int seqOffset = 0;
    HGID parentId = 0;
    int contigIx = 0;
    HGID newParentOrdering = hgNextId();
    struct filePos *faPosList, *pos;
    long faSize;

    getAccFromPath(faName->name, acc);
    sprintf(nameBuf, "%s/%s", faDir, faName->name);
    contigList = faReadAllDna(nameBuf);
    faParseRecords(nameBuf,  &faPosList, &faSize);

    /* Delete old contigs and old orders. */
    sprintf(query, "SELECT id from seq where acc='%s'", acc);
    bacId = hgIdQuery(conn, query);
    sprintf(query, "SELECT id from contig where bac=%lu", bacId);
    sr = sqlGetResult(conn,query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	HGID contigId = sqlUnsigned(row[0]);
	if (parentId == 0)
	    {
	    sprintf(query, "SELECT parent from ordering where contig=%lu", contigId);
	    parentId = hgIdQuery(delConn, query);
	    sprintf(query, "DELETE from ordering where id = %lu", parentId);
	    sqlUpdate(delConn, query);
	    }
	sprintf(query, "DELETE from ordering where contig = %lu", contigId);
	sqlUpdate(delConn, query);
	}
    sqlFreeResult(&sr);
    sprintf(query, "DELETE from contig where bac=%lu", bacId);
    sqlUpdate(conn, query);

    /* Insert new contigs. */
    for (contig = contigList, pos = faPosList; contig != NULL; contig = contig->next, pos = pos->next)
	{
	HGID contigId = hgNextId();
	sprintf(query, "INSERT contig VALUES(%lu,%lu,%d,%d,%d,%ld,%d)",
	    contigId, bacId, contigIx, seqOffset, contig->size, pos->offset, pos->size);
	sqlUpdate(conn, query);
	sprintf(query, "INSERT ordering VALUES(%lu,%lu,%d,1,%d,%d,%lu)",
	    hgNextId(), newParentOrdering, contigIx, seqOffset, contig->size, contigId);
	sqlUpdate(conn, query);
	    
	seqOffset += contig->size;
	if (contig->next != NULL)
	    seqOffset += 800;
	++contigIx;
	}


    /* Update sequence entry. */
    sprintf(query, 
        "UPDATE seq SET size=%d, extFile=%ld, gb_date=NOW(), file_size=%ld "
	"WHERE id = %lu",
	seqOffset, extFileId, fileSize(nameBuf), bacId);
    sqlUpdate(conn, query);

    /* Update bac entry. */
    sprintf(query, "UPDATE bac SET fragments=%d,ordering=%lu WHERE id = %lu",
	slCount(contigList), newParentOrdering, bacId);
    sqlUpdate(conn, query);

    /* Insert new parent ordering. */
    sprintf(query, "INSERT ordering VALUES(%lu,0,0,1,0,%d,0)",
	newParentOrdering, seqOffset);
    sqlUpdate(conn, query);

    freeDnaSeqList(&contigList);
    slFreeList(&faPosList);
    }

sqlDisconnect(&delConn);
slFreeList(&faNameList);
hgEndUpdate(&conn, "Update BACS from %s", faDir);
}

void addRna(char *faName, char *symFaName, char *raName, char *type)
/* Add in RNA data from fa and ra files. */
{
struct sqlConnection *conn = hgStartUpdate();
struct hash *raFieldHash = newHash(8);
struct lineFile *raLf, *faLf;
char *faLine;
int faLineSize;
char faAcc[32];
char nameBuf[512];
DNA *faDna;
int faSize;
long extFileId = storeExtFilesTable(conn, symFaName, faName);
boolean gotFaStart = FALSE;
char *raTag;
char *raVal;
int raLineSize;
int maxMod = 200;
int mod = maxMod;
int lineMaxMod = 50;
int lineMod = lineMaxMod;
int count = 0;
FILE *mrnaTab = hgCreateTabFile("mrna");
FILE *seqTab = hgCreateTabFile("seq");
struct uniqueTable *uniSrc, *uniOrg, *uniLib, *uniClo, *uniSex,
                   *uniTis, *uniDev, *uniCel, *uniCds, *uniGen,
		   *uniPro;

hashAdd(raFieldHash, "src", uniSrc = getUniqueTable(conn, "source", "src"));
hashAdd(raFieldHash, "org", uniOrg = getUniqueTable(conn, "organism", "org"));
hashAdd(raFieldHash, "lib", uniLib = getUniqueTable(conn, "library", "lib"));
hashAdd(raFieldHash, "clo", uniClo = getUniqueTable(conn, "mrnaClone", "clo"));
hashAdd(raFieldHash, "sex", uniSex = getUniqueTable(conn, "sex", "sex"));
hashAdd(raFieldHash, "tis", uniTis = getUniqueTable(conn, "tissue", "tis"));
hashAdd(raFieldHash, "dev", uniDev = getUniqueTable(conn, "development", "dev"));
hashAdd(raFieldHash, "cel", uniCel = getUniqueTable(conn, "cell", "cel"));
hashAdd(raFieldHash, "cds", uniCds = getUniqueTable(conn, "cds", "cds"));
hashAdd(raFieldHash, "gen", uniGen = getUniqueTable(conn, "geneName", "gen"));
hashAdd(raFieldHash, "pro", uniPro = getUniqueTable(conn, "productName", "pro"));

/* Open input files. */
faLf = lineFileOpen(faName, TRUE);
raLf = lineFileOpen(raName, TRUE);

/* Seek to first line starting with '>' in line file. */
if (!faSeekNextRecord(faLf))
    errAbort("%s doesn't appear to be an .fa file\n", faName);
lineFileReuse(faLf);

/* Loop around for each record of FA and RA */
for (;;)
    {
    char dir = '0';
    boolean gotRaAcc = FALSE, gotRaDate = FALSE;
    HGID id;
    unsigned long faOffset, faEndOffset;
    int faSize;
    boolean atEof;
    char *s;
    int faNameSize;
    char sqlDate[32];
    int dnaSize = 0;

    ++count;
    if (--mod == 0)
	{
	printf(".");
	fflush(stdout);
	mod = maxMod;
	if (--lineMod == 0)
	    {
	    printf("%d\n", count);
	    lineMod = lineMaxMod;
	    }
	}
    /* Get Next FA record. */
    if (!lineFileNext(faLf, &faLine, &faLineSize))
	break;
    if (faLine[0] != '>')
	internalErr();
    faOffset = faLf->bufOffsetInFile + faLf->lineStart;
    s = firstWordInLine(faLine+1);
    faNameSize = strlen(s);
    if (faNameSize == 0)
	errAbort("Missing accession line %d of %s", faLf->lineIx, faLf->fileName);
    if (strlen(faLine+1) >= sizeof(faAcc))
	errAbort("FA name too long line %d of %s", faLf->lineIx, faLf->fileName);
    strcpy(faAcc, s);
    if (faSeekNextRecord(faLf))
	lineFileReuse(faLf);
    faEndOffset = faLf->bufOffsetInFile + faLf->lineStart;
    faSize = (int)(faEndOffset - faOffset); 

    /* Get next RA record. */
    clearUniqueIds();
    for (;;)
	{
	struct hashEl *hel;
	if (!lineFileNext(raLf, &raTag, &raLineSize))
	    errAbort("Unexpected eof in %s", raName);
	if (raTag[0] == 0)
	    break;
	raVal = strchr(raTag, ' ');
	if (raVal == NULL)
	    errAbort("Badly formatted tag line %d of %s", raLf->lineIx, raLf->fileName);
	*raVal++ = 0;
	if ((hel = hashLookup(raFieldHash, raTag)) != NULL)
	    {
	    struct uniqueTable *uni = hel->val;
	    uni->curId = uniqueStore(conn,uni,raVal);
	    }
	else if (sameString(raTag, "acc"))
	    {
	    gotRaAcc = TRUE;
	    s = firstWordInLine(raVal);
	    if (!sameString(s, faAcc))
		errAbort("Accession mismatch %s and %s between %s and %s",
		    faAcc, raVal, faName, raName);
	    }
	else if (sameString(raTag, "dir"))
	    {
	    dir = raVal[0];
	    }
	else if (sameString(raTag, "dat"))
	    {
	    gbDateToSqlDate(raVal, sqlDate);
	    gotRaDate = TRUE;
	    }
	else if (sameString(raTag, "siz"))
	    {
	    dnaSize = sqlUnsigned(raVal);
	    }
	}

    /* Do a little error checking and then write out to tables. */
    if (!gotRaAcc)
	errAbort("No accession in %s\n", raName);
    if (!gotRaDate)
	errAbort("No date in %s\n", faAcc);
    if (!dnaSize)
	errAbort("No size in %s\n", faAcc);
    id = hgNextId();

    fprintf(seqTab, "%lu\t%s\t%d\t%s\t%lu\t%lu\t%d\n",
	id, faAcc, dnaSize, sqlDate, extFileId, faOffset, faSize);
    fprintf(mrnaTab, "%lu\t%s\t%s\t%c\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\n",
	id, faAcc, type, dir,
	uniSrc->curId, uniOrg->curId, uniLib->curId, uniClo->curId,
	uniSex->curId, uniTis->curId, uniDev->curId, uniCel->curId,
	uniCds->curId, uniGen->curId, uniPro->curId);
    }
printf("%d\n", count);
storeUniqueTables(conn);
lineFileClose(&faLf);
lineFileClose(&raLf);
fclose(mrnaTab);
fclose(seqTab);
hgLoadTabFile(conn, "mrna");
hgLoadTabFile(conn, "seq");
hgEndUpdate(&conn, "Add mRNA from %s,%s", faName, raName);
}


void otest()
/* Do something, who knows exactly what. */
{
struct sqlConnection *conn = sqlConnect(database);
sqlDisconnect(&conn);
}

void test()
/* Do something, who knows exactly what. */
{
char *bacName = "AC000001";
struct hgBac *bac = hgGetBac(bacName);
struct dnaSeq *seq = hgBacOrderedSeq(bac);
faWrite("/c/temp/AC000001.fa", bacName, seq->dna, seq->size);
freeDnaSeq(&seq);
printf("Wrote out %s\n", bacName);

seq = hgBacSubmittedSeq(bacName);
faWrite("/c/temp/AC000001.fa.2", bacName, seq->dna, seq->size);
freeDnaSeq(&seq);
printf("Wrote out second %s\n", bacName);
}

void dropAll()
/* Drop all tables from database. */
{
struct sqlConnection *conn = sqlConnect(database);
sqlUpdate(conn, "drop database hgap");
sqlDisconnect(&conn);
}

void createAll()
/* Create all tables in database. */
{
struct sqlConnection *conn = sqlConnect(database);
int i;
char *table;

sqlUpdate(conn, historyTable);
sqlUpdate(conn, "INSERT into history VALUES(NULL,0,10,USER(),'New',NOW())");
sqlUpdate(conn, extFileTable);
sqlUpdate(conn, seqTable);
sqlUpdate(conn, mrnaTable);
sqlUpdate(conn, bacTable);
sqlUpdate(conn, contigTable);
sqlUpdate(conn, orderingTable);
for (i=0; i<ArraySize(uniqueTableNames); ++i)
    {
    char query[256];
    table = uniqueTableNames[i];
    createUniqTable(conn, table);
    sprintf(query, "INSERT into %s VALUES(0,'n/a')", table);
    sqlUpdate(conn, query);
    }
sqlDisconnect(&conn);
}

void usage()
/* Print usage instructions and exit. */
{
errAbort(
"loadDb - Help load data into hgap mySQL database\n"
"usage:\n"
"    loadDb delete\n"
"This deletes the existing database and resets it.\n"
"    loadDb new\n"
"Create empty tables.\n"
"    loadDb addRna faFile raFile\n"
"Add in full length RNA data\n"
"    loadDb addEst faFile estFile\n"
"Add in EST data\n"
"    loadDb addBac symName faDir/ raFile\n"
"Loads in BAC data from directory\n"
"    loadDb updateBac symName faDir/ \n"
"Updates seq, bac, and contig tables from bacs in dir.\n");
#ifdef OLDWAY
"    loadDb cdnaAli file.sf\n"
"Loads alignments from .sf file.\n"
"    loadDb addBlastx files(s).blastx\n"
"Loads blastx alignments from files.\n"
"    loadDb addAllBlastx dir suffix\n"
"Adds all blastx files with given suffix\n");
#endif /* OLDWAY */
}

int main(int argc, char *argv[])
{
char *command;

if (argc < 2)
    usage();
pushDebugAbort();
dnaUtilOpen();
hgSetDb(database);
command = argv[1];
if (sameWord(command, "delete"))
    {
    dropAll();
    }
else if (sameWord(command, "new"))
    {
    createAll();
    }
else if (sameWord(command, "test"))
    {
    test();
    }
else if (sameWord(command, "addRna"))
    {
    if (argc != 4)
	usage();
    addRna(argv[2], "mRNA", argv[3], "mRNA");
    }
else if (sameWord(command, "addEst"))
    {
    if (argc != 4)
	usage();
    addRna(argv[2], "EST", argv[3], "EST");
    }
else if (sameWord(command, "addBac"))
    {
    if (argc != 5)
	usage();
    addBac(argv[2], argv[3], argv[4]);
    }
else if (sameWord(command, "updateBac"))
    {
    if (argc != 4)
	usage();
    updateBac(argv[2], argv[3]);
    }
#ifdef OLDWAY
else if (sameWord(command, "cdnaAli"))
    {
    if (argc != 3)
	usage();
    addCdnaAli(argv[2]);
    }
else if (sameWord(command, "addBlastx"))
    {
    if (argc < 3)
	usage();
    addBlastxFiles(argv+2, argc-2, &btiDefault);
    }
else if (sameWord(command, "addAllBlastx"))
    {
    if (argc != 4)
	usage();
    addBlastxDir(argv[2], argv[3], &btiDefault);
    }
#endif /* OLDWAY */
}
