/* libScan - Scan libraries to help find g' capped ones. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "htmshell.h"


int dotEvery = 0;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "libScan - Scan libraries to help find g' capped ones\n"
  "usage:\n"
  "   libScan database output.html\n"
  "options:\n"
  "   -dots=N - write a dot every N mrnas/ests\n"
  );
}

char *entrezScript = "http://www.ncbi.nlm.nih.gov/htbin-post/Entrez/query?form=4";

void printEntrezNucleotideUrl(FILE *f, char *accession)
/* Print URL for Entrez browser on a nucleotide. */
{
fprintf(f, "\"%s&db=n&term=%s\"", entrezScript, accession);
}


void dotOut()
/* Put out a dot every now and then if user want's to. */
{
static int mod = 1;
if (dotEvery > 0)
    {
    if (--mod <= 0)
	{
	fputc('.', stdout);
	fflush(stdout);
	mod = dotEvery;
	}
    }
}


struct libInfo
/* Info on one library. */
    {
    struct libInfo *next;
    char *idString;	/* Id as a string for hashing. */
    char *name;		/* Library name. */
    char authorId[10];	/* Author ID - checked to make sure unique. */
    boolean multiAuthor;/* More than one author? */
    int mrnaCount;	/* Number of mRNAs used. */
    int estCount;	/* Number of ESTs used. */
    char sampleAcc[12];		/* Accession of one instance of library. */
    char sampleDate[12];	/* Date of sample. */
    };

boolean isOrestesForm(char *name)
/* Is lib name in format that shows it's from Orestes project? */
{
int i, len = strlen(name);
if (len < 5 || len > 6)
    return FALSE;
if (!isupper(name[0]))
    return FALSE;
if (!isupper(name[1]))
    return FALSE;
for (i=2; i<len; ++i)
    if (!isdigit(name[i]))
	return FALSE;
return TRUE;
}

int libInfoCmpEstCount(const void *va, const void *vb)
/* Compare to sort based on query. */
{
const struct libInfo *a = *((struct libInfo **)va);
const struct libInfo *b = *((struct libInfo **)vb);
return b->estCount - a->estCount;
}


void libScan(char *database, char *outName)
/* libScan - Scan libraries to help find g' capped ones. */
{
struct sqlConnection *conn = sqlConnect(database);
char query[256];
char **row;
struct sqlResult *sr;
struct libInfo *libList = NULL, *lib;
struct hash *libHash = newHash(0);
FILE *f = mustOpen(outName, "w");
boolean isEst;

sr = sqlGetResult(conn, NOSQLINJ "select id,name from library");
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(lib);
    slAddHead(&libList, lib);
    hashAddSaveName(libHash, row[0], lib, &lib->idString);
    lib->name = cloneString(row[1]);
    }
sqlFreeResult(&sr);
slReverse(&libList);
printf("Got %d libraries in %s\n", slCount(libList), database);

sr = sqlGetResult(conn, NOSQLINJ "select type,library,author,acc from mrna");
while ((row = sqlNextRow(sr)) != NULL)
    {
    dotOut();
    lib = hashMustFindVal(libHash, row[1]);
    isEst = sameString(row[0], "EST");
    if (lib->estCount + lib->mrnaCount == 0)
	{
	strncpy(lib->sampleAcc, row[3], sizeof(lib->sampleAcc));
	strncpy(lib->authorId, row[2], sizeof(lib->authorId));
	}
    else
	{
	if (isEst && lib->estCount == 0)
	    strncpy(lib->sampleAcc, row[3], sizeof(lib->sampleAcc));
	if (!sameString(lib->authorId, row[2]))
	    {
	    if (!lib->multiAuthor)
		{
		lib->multiAuthor = TRUE;
		}
	    }
	}
    if (isEst)
        ++lib->estCount;
    else
        ++lib->mrnaCount;
    }
if (dotEvery)
   printf("\n");
sqlFreeResult(&sr);
slSort(&libList, libInfoCmpEstCount);

htmStart(f, "cDNA Libraries");
fprintf(f, "<PRE>");
for (lib = libList; lib != NULL; lib = lib->next)
    {
    if (lib->estCount > 50 && !isOrestesForm(lib->name))
	{
	char date[16];
	sqlSafef(query, sizeof query, "select gb_date from seq where acc = '%s'", lib->sampleAcc);
	sqlQuickQuery(conn, query, date, sizeof(date));
	if (!startsWith("0000", date) && strcmp(date, "1999") > 0)
	    {
	    fprintf(f, "<A HREF=");
	    printEntrezNucleotideUrl(f, lib->sampleAcc);
	    fprintf(f, ">%s</A> ", lib->sampleAcc);
	    fprintf(f, "%s %d\t%d\t%d\t%s\n", date, lib->multiAuthor, lib->mrnaCount, lib->estCount, lib->name);
	    }
	}
    }
htmEnd(f);

sqlDisconnect(&conn);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
dotEvery = cgiOptionalInt("dots", dotEvery);
if (argc != 3)
    usage();
libScan(argv[1], argv[2]);
return 0;
}
