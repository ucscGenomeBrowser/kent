/* hgLoadRna - load browser database with mRNA/EST info.. */

/* This is derived from a system that used to apply to the whole
 * database, but currently only applies to the mRNA/EST bits -
 * where each record has a globally uniq ID across the entire
 * database.  
 *
 * This system ended up being more complex than it was worth
 * in my judgement.  We couldn't simply suck in a tab-separated
 * file as a table without haveing to put in an extra ID field.
 *
 * However the system provides some useful services for the
 * mRNA/EST data.  In particular this data consists of many
 * fields such as library, tissue, which have values shared
 * by many records.  The "uniqTable" routines and the
 * id's in the "history table" that they depend upone
 * let us store each unique value once, and then just
 * reference these values in the larger records. */

#include "common.h"
#include "portable.h"
#include "linefile.h"
#include "hash.h"
#include "fa.h"
#include "hgRelate.h"

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
  "keyword int unsigned not null,"                /* Ref in key table. */
  "description int unsigned not null,"            /* Ref in description table. */
  "geneName int unsigned not null,"               /* Ref in geneName table. */
  "productName int unsigned not null,"            /* Ref in productName table. */
  "author int unsigned not null,"                 /* Ref in author table. */
	   /* Extra indices. */
  "unique (acc),"
  "index (type),"
  "index (library),"
  "index (mrnaClone),"
  "index (tissue),"
  "index (development),"
  "index (cell),"
  "index (keyword),"
  "index (description),"
  "index (geneName),"
  "index (productName),"
  "index (author))"
  ;

char *uniqueTableNames[] =
    {
    "development", "cell", "cds", "geneName", "productName",
    "source", "organism", "library", "mrnaClone", "sex", "tissue",
    "center", "bacClone", "cytoMap", "chromosome", "author", "keyword", "description",
    };

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

void createUniqTable(struct sqlConnection *conn, char *tableName)
/* Create a simple ID/Name pair table. */
{
char query[256];
sprintf(query,
    "create table %s (" 
       "id int not null primary key,"
       "name varchar(255) not null,"
       "index (name(16)))",
    tableName); 
sqlUpdate(conn, query);
}

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

static char *nullPt = NULL;

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

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadRna - load browser database with mRNA/EST info.\n"
  "usage:\n"
  "   hgLoadRna new database\n"
  "This creates freshly the RNA part of the database\n"
  "   hgLoadRna add database /full/path/mrna.fa mrna.ra\n"
  "This adds mrna info to the database\n"
  "   hgLoadRna drop database\n"
  "This drops the tables created by hgLoadRna from database.\n"
  );
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
		   *uniPro, *uniAut, *uniKey, *uniDef;

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
hashAdd(raFieldHash, "aut", uniAut = getUniqueTable(conn, "author", "aut"));
hashAdd(raFieldHash, "key", uniKey = getUniqueTable(conn, "keyword", "key"));
if (sameWord(type, "mRNA"))
    {
    hashAdd(raFieldHash, "def", uniDef = getUniqueTable(conn, "description", "def"));
    }
else
    uniDef = getUniqueTable(conn, "description", "def");

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
    fprintf(mrnaTab, "%lu\t%s\t%s\t%c\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\n",
	id, faAcc, type, dir,
	uniSrc->curId, uniOrg->curId, uniLib->curId, uniClo->curId,
	uniSex->curId, uniTis->curId, uniDev->curId, uniCel->curId,
	uniCds->curId, uniKey->curId, uniDef->curId, uniGen->curId, uniPro->curId, uniAut->curId);
    }
printf("%d\n", count);
printf("Updating tissue, lib, etc. values\n");
storeUniqueTables(conn);
lineFileClose(&faLf);
lineFileClose(&raLf);
fclose(mrnaTab);
fclose(seqTab);
printf("Updating mrna table\n");
hgLoadTabFile(conn, "mrna");
printf("Updating seq table\n");
hgLoadTabFile(conn, "seq");
hgEndUpdate(&conn, "Add mRNA from %s,%s", faName, raName);
printf("All done\n");
}


void hgLoadRna(char *database, char *faPath, char *raFile)
/* hgLoadRna - load browser database with mRNA/EST info.. */
{
char *type, *symName;
hgSetDb(database);
if (strstr(raFile, "est.ra"))
    type = "EST";
else
    type = "mRNA";
addRna(faPath, type, raFile, type);
}

void createAll(char *database)
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

void dropAll(char *database)
/* Drop all hgLoadRna tables from database. */
{
struct sqlConnection *conn = sqlConnect(database);
char *table;
int i;

sqlUpdate(conn, "drop table history");
sqlUpdate(conn, "drop table extFile");
sqlUpdate(conn, "drop table seq");
sqlUpdate(conn, "drop table mrna");
for (i=0; i<ArraySize(uniqueTableNames); ++i)
    {
    char query[256];
    table = uniqueTableNames[i];
    sprintf(query, "drop table %s", table);
    sqlUpdate(conn, query);
    }
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *command;

if (argc < 2)
    usage();
command = argv[1];
if (sameString(command, "new"))
    {
    if (argc != 3)
        usage();
    createAll(argv[2]);
    }
else if (sameString(command, "drop"))
    {
    if (argc != 3)
        usage();
    dropAll(argv[2]);
    }
else if (sameString(command, "add"))
    {
    if (argc != 5)
        usage();
    hgLoadRna(argv[2], argv[3], argv[4]);
    }
else
    usage();
return 0;
}
