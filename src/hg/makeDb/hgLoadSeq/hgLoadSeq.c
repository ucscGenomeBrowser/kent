/* hgLoadSeq - load sequences into the seq/extFile tables. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "options.h"
#include "portable.h"
#include "linefile.h"
#include "hash.h"
#include "fa.h"
#include "hgRelate.h"


/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"abbr", OPTION_STRING},
    {"prefix", OPTION_STRING},
    {"replace", OPTION_BOOLEAN},
    {"drop", OPTION_BOOLEAN},
    {"test", OPTION_BOOLEAN},
    {"seqTbl", OPTION_STRING},
    {"extFileTbl", OPTION_STRING},
    {NULL, 0}
};

/* Command line options and defaults. */
char *seqTbl = "seq";
char *extFileTbl = "extFile";
char *abbr = NULL;
char *prefix = NULL;
boolean test = FALSE;
boolean replace = FALSE;
boolean drop = FALSE;

char seqTableCreate[] =
/* This keeps track of a sequence. */
"create table %s ("
  "id int unsigned not null primary key," /* Unique ID across all tables. */
  "acc varchar(128) not null ,"	          /* seq  ID. */
  "size int unsigned not null,"           /* Size of sequence in bases. */
  "gb_date date not null,"                /* GenBank last modified date,
                                           * not used, for compatbility with older databases. */
  "extFile int unsigned not null,"        /* File it is in. */
  "file_offset bigint not null,"          /* Offset in file. */
  "file_size int unsigned not null,"      /* Size in file. */
  /* Extra indices. */
  "unique (acc))";

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

void abbreviate(char *s, char *fluff)
/* Cut out fluff from s. */
{
int len;
if (s != NULL && fluff != NULL)
    {
    s = strstr(s, fluff);
    if (s != NULL)
       {
       len = strlen(fluff);
       strcpy(s, s+len);
       }
    }
}

boolean readFaSeq(struct lineFile *faLf, char **retFaName,  int *retDnaSize, off_t *retFaOffset)
/* Read the next record, returning it's start location in the file */
{
// to get offset, must read first line, save offset, then read the record to
// get the size
char *faLine;
if (!lineFileNext(faLf, &faLine, NULL))
    return FALSE;
if (faLine[0] != '>')
    errAbort("fasta record doesn't start with '>' line %d of %s", faLf->lineIx, faLf->fileName);
*retFaOffset = faLf->bufOffsetInFile + faLf->lineStart;
lineFileReuse(faLf);
DNA *dna;
boolean gotIt = faMixedSpeedReadNext(faLf, &dna, retDnaSize, retFaName);
if (!gotIt)
    internalErr();
return TRUE;
}

boolean loadFaSeq(struct lineFile *faLf, HGID extFileId, HGID seqId, FILE *seqTab)
/* Add next sequence in fasta file to tab file */
{
off_t faOffset, faEndOffset;
int faSize, dnaSize;
char *faName, faAcc[256], faAccBuf[513];
int prefixLen = 0;

/* Get next FA record. */
if (!readFaSeq(faLf, &faName, &dnaSize, &faOffset))
    return FALSE;
char *s = firstWordInLine(faName);
abbreviate(s, abbr);
if (strlen(s) == 0)
    errAbort("Missing accession line %d of %s", faLf->lineIx, faLf->fileName);
if (prefix != NULL)
    prefixLen = strlen(prefix) + 1;
if (strlen(faName+1) + prefixLen >= sizeof(faAcc))
    errAbort("Fasta name too long line %d of %s", faLf->lineIx, faLf->fileName);
faAcc[0] = 0;
if (prefix != NULL)
    {
    safecat(faAcc, sizeof(faAcc), prefix);
    safecat(faAcc, sizeof(faAcc), "-");
    }
strcat(faAcc, s);
faEndOffset = faLf->bufOffsetInFile + faLf->lineStart;
faSize = (int)(faEndOffset - faOffset); 

/* note: sqlDate column is empty */
fprintf(seqTab, "%u\t%s\t%d\t0000-00-00\t%u\t%lld\t%d\n",
        seqId, sqlEscapeTabFileString2(faAccBuf, faAcc),
        dnaSize, extFileId, (unsigned long long)faOffset, faSize);
return TRUE;
}

void loadFa(char *faFile, struct sqlConnection *conn, FILE *seqTab, HGID *nextSeqId)
/* Add sequences in a fasta file to a seq table tab file */
{
/* Check if the faFile is already in the extFileTbl and inform the user.*/   
char query[1024]; 
sqlSafef(query, sizeof(query), "(select * from %s where name='%s'",extFileTbl,faFile);
if ((!test) && (!replace) && (sqlGetResultExt(conn, query, NULL, NULL) != NULL))
    errAbort("The file %s already has an entry in %s. To replace the existing entry rerun with the "
	    "-replace option.", faFile, extFileTbl); 

HGID extFileId = test ? 0 : hgAddToExtFileTbl(faFile, conn, extFileTbl);
struct lineFile *faLf = lineFileOpen(faFile, TRUE);
unsigned count = 0;

verbose(1, "Adding %s\n", faFile);

/* Seek to first line starting with '>' in line file. */
if (!faSeekNextRecord(faLf))
    errAbort("%s doesn't appear to be an .fa file\n", faLf->fileName);
lineFileReuse(faLf);

/* Loop around for each record of FA */
while (loadFaSeq(faLf, extFileId, *nextSeqId, seqTab))
    {
    (*nextSeqId)++;
    count++;
    }

verbose(1, "%u sequences\n", count);
lineFileClose(&faLf);
}

void hgLoadSeq(char *database, int fileCount, char *fileNames[])
/* Add a bunch of FA files to sequence and extFile tables of
 * database. */
{
struct sqlConnection *conn;
int i;
FILE *seqTab;
HGID firstSeqId = 0, nextSeqId = 0;

if (!test)
    {
    conn = hgStartUpdate(database);
    char query[1024];
    if (drop)
        {
        sqlSafef(query, sizeof(query), "drop table if exists %s", seqTbl);
        sqlUpdate(conn, query);
        sqlSafef(query, sizeof(query), "drop table if exists %s", extFileTbl);
        sqlUpdate(conn, query);
        }
    sqlSafef(query, sizeof(query), seqTableCreate, seqTbl);
    // This aught to catch the duplicate table entry 
    sqlMaybeMakeTable(conn, seqTbl, query);
    firstSeqId = nextSeqId = hgGetMaxId(conn, seqTbl) + 1;
    }

verbose(1, "Creating %s.tab file\n", seqTbl);
seqTab = hgCreateTabFile(".", seqTbl);
for (i=0; i<fileCount; ++i)
    {
    loadFa(fileNames[i], conn, seqTab, &nextSeqId);
    }
if (!test)
    {
    unsigned opts = 0;
    if (replace)
        opts |= SQL_TAB_REPLACE;
    verbose(1, "Updating %s table\n", seqTbl);
    hgLoadTabFileOpts(conn, ".", seqTbl, opts, &seqTab);
    hgEndUpdate(&conn, firstSeqId, nextSeqId-1, "Add sequences to %s from %d files starting with %s", 
                seqTbl, fileCount, fileNames[0]);
    verbose(1, "All done\n");
    }
}

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadSeq - load browser database with sequence file info.\n"
  "usage:\n"
  "   hgLoadSeq [-abbr=junk] database file(s).fa\n"
  "This loads sequence file info only, it is not used for genbank data.\n"
  "\n"
  "Options:\n"
  "  -abbr=junk - remove junk from the start of each seq accession\n"
  "  -prefix=xxx - prepend \"xxx-\" to each seq accession\n"
  "  -replace - replace existing sequences with the same id\n"
  "  -seqTbl=tbl - use this table instead of seq\n"
  "  -extFileTbl=tbl - use this table instead of extFile\n"
  "  -test - do not load database table\n"
  "  -drop - drop tables before loading, can only use if -seqTbl and -extFileTbl\n"
  "   are specified. \n"
  );
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc < 2)
    usage();
if ((optionExists("seqTbl") && !optionExists("extFileTbl"))
    || (!optionExists("seqTbl") && optionExists("extFileTbl")))
    errAbort("must specified both or neither of -seqTbl and -extFileTbl");
seqTbl = optionVal("seqTbl", seqTbl);
extFileTbl = optionVal("extFileTbl", extFileTbl);
abbr = optionVal("abbr", NULL);
prefix = optionVal("prefix", NULL);
replace = optionExists("replace");
test = optionExists("test");
drop = optionExists("drop");
if (drop && !optionExists("seqTbl"))
    errAbort("can only specify -drop with -seqTbl and -extFileTbl");
hgLoadSeq(argv[1], argc-2, argv+2);
return 0;
}
