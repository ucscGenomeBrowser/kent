/* hgLoadSeq - load sequences into the seq/extFile tables. */

#include "common.h"
#include "options.h"
#include "portable.h"
#include "linefile.h"
#include "hash.h"
#include "fa.h"
#include "hgRelate.h"

static char const rcsid[] = "$Id: hgLoadSeq.c,v 1.4 2003/11/04 15:48:37 kate Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"abbr", OPTION_STRING},
    {NULL, 0}
};

/* Command line options and defaults. */
char *abbr = NULL;

char seqTable[] =
/* This keeps track of a sequence. */
"create table seq ("
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

boolean loadFaSeq(struct lineFile *faLf, HGID extFileId, FILE *seqTab)
/* Add next sequence in fasta file to tab file */
{
off_t faOffset, faEndOffset;
int faSize;
char *s, *faLine;
int faLineSize, faNameSize;
int dnaSize = 0;
char faAcc[32];
HGID seqId;

/* Get Next FA record. */
if (!lineFileNext(faLf, &faLine, &faLineSize))
    return FALSE;;
if (faLine[0] != '>')
    internalErr();
faOffset = faLf->bufOffsetInFile + faLf->lineStart;
s = firstWordInLine(faLine+1);
abbreviate(s, abbr);
faNameSize = strlen(s);
if (faNameSize == 0)
    errAbort("Missing accession line %d of %s", faLf->lineIx, faLf->fileName);
if (strlen(faLine+1) >= sizeof(faAcc))
    errAbort("Fasta name too long line %d of %s", faLf->lineIx, faLf->fileName);
strcpy(faAcc, s);
if (faSeekNextRecord(faLf))
    lineFileReuse(faLf);
faEndOffset = faLf->bufOffsetInFile + faLf->lineStart;
faSize = (int)(faEndOffset - faOffset); 

seqId = hgNextId();

/* note: sqlDate column is empty */
fprintf(seqTab, "%u\t%s\t%d\t\t%u\t%lld\t%d\n",
        seqId, faAcc, dnaSize, extFileId, faOffset, faSize);
return TRUE;
}

void loadFa(char *faFile, struct sqlConnection *conn, FILE *seqTab)
/* Add sequences in a fasta file to a seq table tab file */
{
HGID extFileId = hgAddToExtFile(faFile, conn);
struct lineFile *faLf = lineFileOpen(faFile, TRUE);
unsigned count = 0;

printf("Adding %s\n", faFile);

/* Seek to first line starting with '>' in line file. */
if (!faSeekNextRecord(faLf))
    errAbort("%s doesn't appear to be an .fa file\n", faLf->fileName);
lineFileReuse(faLf);

/* Loop around for each record of FA */
while (loadFaSeq(faLf, extFileId, seqTab))
    count++;

printf("%u sequences\n", count);
lineFileClose(&faLf);
}

void hgLoadSeq(char *database, int fileCount, char *fileNames[])
/* Add a bunch of FA files to sequence and extFile tables of
 * database. */
{
struct sqlConnection *conn;
int i;
FILE *seqTab;

hgSetDb(database);
conn = hgStartUpdate();
sqlMaybeMakeTable(conn, "seq", seqTable);

seqTab = hgCreateTabFile(".", "seq");
for (i=0; i<fileCount; ++i)
    {
    loadFa(fileNames[i], conn, seqTab);
    }
printf("Updating seq table\n");
hgLoadTabFile(conn, ".", "seq", &seqTab);
hgEndUpdate(&conn, "Add sequences");
printf("All done\n");
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
  "  -abbr=junk - remove junk from the start of each seq accesions\n"
  );
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc < 2)
    usage();
abbr = optionVal("abbr", NULL);
hgLoadSeq(argv[1], argc-2, argv+2);
return 0;
}
