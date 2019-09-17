/* bedExtendRanges - extend bed item sizes. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "options.h"
#include "linefile.h"
#include "obscure.h"
#include "hash.h"
#include "jksql.h"
#include "dystring.h"
#include "chromInfo.h"
#include "hdb.h"
#include "portable.h"


static boolean strictTab = FALSE;	/* Separate on tabs. */
static struct hash *chromHash = NULL;
char *host = NULL;
char *user = NULL;
char *password = NULL;

static struct optionSpec optionSpecs[] = {
    {"tab", OPTION_BOOLEAN},
    {"host", OPTION_STRING},
    {"user", OPTION_STRING},
    {"password", OPTION_STRING},
    {NULL, 0}
};

/* Command line switches. */
void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedExtendRanges - extend length of entries in bed 6+ data to be at least the given length,\n"
  "taking strand directionality into account.\n\n"
  "usage:\n"
  "   bedExtendRanges database length files(s)\n\n"
  "options:\n"
  "   -host\tmysql host\n"
  "   -user\tmysql user\n"
  "   -password\tmysql password\n"
  "   -tab\t\tSeparate by tabs rather than space\n"
  "   -verbose=N - verbose level for extra information to STDERR\n\n"
  "example:\n\n"
  "   bedExtendRanges hg18 250 stdin\n\n"
  "   bedExtendRanges -user=genome -host=genome-mysql.soe.ucsc.edu hg18 250 stdin\n\n"
  "will transform:\n"
  "    chr1 500 525 . 100 +\n" 
  "    chr1 1000 1025 . 100 -\n" 
  "to:\n"
  "    chr1 500 750 . 100 +\n"
  "    chr1 775 1025 . 100 -\n"
  );
}

static struct hash *loadAllChromInfo(char *database)
/* Load up all chromosome infos. */
{
struct chromInfo *el;
struct sqlConnection *conn = NULL;
struct sqlResult *sr = NULL;
struct hash *ret;
char **row;
boolean localDb = TRUE;

if(host)
    {
    conn = sqlConnectRemote(host, user, password, database);
    localDb = FALSE;
    }
else
    {
    conn = hAllocConn(database);
    }
ret = newHash(0);

sr = sqlGetResult(conn, NOSQLINJ "select * from chromInfo");
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = chromInfoLoad(row);
    verbose(4, "Add hash %s value %u (%#lx)\n", el->chrom, el->size, (unsigned long)&el->size);
    hashAdd(ret, el->chrom, (void *)(& el->size));
    }
sqlFreeResult(&sr);
if(localDb)
    hFreeConn(&conn);
return ret;
}

static unsigned chromosomeSize(char *chromosome)
/* Return full extents of chromosome.  Warn and fill in if none. */
{
struct hashEl *el = hashLookup(chromHash,chromosome);

if (el == NULL)
    errAbort("Couldn't find size of chromosome %s (note: chrom names are case sensitive)", chromosome);
return *(unsigned *)el->val;
}

static void processOneFile(char *fileName, int bedSize, int length)
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[64], *line;
int wordCount;

verbose(1, "Reading %s\n", lf->fileName);
while (lineFileNextReal(lf, &line))
    {
    int i;
    char *dupe = cloneString(line);
    if (strictTab)
	wordCount = chopTabs(line, words);
    else
	wordCount = chopLine(line, words);
    /* ignore empty lines	*/
    if (0 == wordCount)
	continue;
    lineFileExpectAtLeast(lf, bedSize, wordCount);

    char *chrom = words[0];
    // use signed for chromStart b/c calculations below may (temorarily) yield a negative number.
    int chromStart = lineFileNeedNum(lf, words, 1);
    unsigned chromEnd = lineFileNeedNum(lf, words, 2);
    char *name = words[3];
    int score = lineFileNeedNum(lf, words, 4);
    char *strand = words[5];
    
    if (chromEnd < 1)
        errAbort("ERROR: line %d:'%s'\nchromEnd is less than 1\n",
		     lf->lineIx, dupe);
    if (chromStart > chromEnd)
        errAbort("ERROR: line %d:'%s'\nchromStart after chromEnd (%d > %d)\n",
                 lf->lineIx, dupe, chromStart, chromEnd);

    int delta = length - (chromEnd - chromStart);
    if(delta && delta > 0)
        {
        if(sameString(strand, "+"))
            chromEnd += delta;
        else if (sameString(strand, "-"))
            chromStart -= delta;
        else
            {
            chromStart -= delta / 2;
            chromEnd += delta / 2 + delta % 2;
            }
        }
    chromStart = max(0, chromStart);
    unsigned size = chromosomeSize(chrom);
    // XXXX special-case chrM?
    chromEnd = min(chromEnd, size);

    char sep = strictTab ? '\t' : ' ';
    printf("%s%c%d%c%d%c%s%c%d%c%s", chrom, sep, chromStart, sep, chromEnd, sep, name, sep, score, sep, strand);
    // print out remaining (unprocessed) fields.
    for(i=6;i<wordCount;i++)
        {
        printf("%c%s", sep, words[i]);
        }
    printf("\n");
    freeMem(dupe);
    }
}

static void bedExtendRanges(char *database, int length,
	int count, char *files[])
/* bedExtendRanges - process each file to extend ranges. */
{
int i;

chromHash = loadAllChromInfo(database);

if (verboseLevel() > 2)
    {
    struct hashCookie cookie;
    struct hashEl *el;

    cookie = hashFirst(chromHash);

    verbose(3,"chrom\tsize\n");
    while ((el = hashNext(&cookie)) != NULL)
	{
	unsigned size;
	size = chromosomeSize(el->name);
	verbose(3,"%s\t%u\n", el->name, size);
	}
    }

for (i=0; i<count; ++i)
    processOneFile(files[i], 6, length);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc < 3)
    usage();
strictTab = optionExists("tab");
host = optionVal("host", NULL);
user = optionVal("user", NULL);
password = optionVal("password", NULL);

bedExtendRanges(argv[1], atoi(argv[2]), argc-3, argv+3);
return 0;
}
