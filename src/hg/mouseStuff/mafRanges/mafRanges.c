/* mafRanges - Extract ranges of target (or query) coverage from maf and 
 * output as BED 3 (intended for further processing by featureBits). */

/* Copyright (C) 2004 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "maf.h"
#include "string.h"

/* Command line switches */
char *otherDb = NULL;
boolean notAllOGap = FALSE;

/* Command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"otherDb", OPTION_STRING},
    {"notAllOGap", OPTION_BOOLEAN},
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort(
"mafRanges - Extract ranges of target (or query) coverage from maf and \n"
"            output as BED 3 (e.g. for processing by featureBits).\n"
"usage:\n"
"   mafRanges in.maf db out.bed\n"
"            db should appear in in.maf alignments as the first part of \n"
"            \"db.seqName\"-style sequence names.  The seqName part will \n"
"            be used as the chrom field in the range printed to out.bed.\n"
"options:\n"
"   -otherDb=oDb  Output ranges only for alignments that include oDb.\n"
"                 oDB can be comma-separated list.\n"
"   -notAllOGap   Don't include bases for which all other species have a gap.\n"
"\n"
);
}


struct hash *hashDbs(char *db, char *otherDb)
/* Make a hash with just the names of the species that we're interested in,
 * so we can quickly look up maf component names later. */
{
struct hash *hash = newHash(8);
char *words[256];

hashStoreName(hash, db);
if (otherDb != NULL)
    {
    int wordCount = chopCommas(otherDb, words);
    int i;
    for (i=0;  i < wordCount;  i++)
	{
	hashStoreName(hash, words[i]);
	}
    }
return hash;
}


boolean gotDbs(struct mafAli *ma, struct hash *dbHash)
/* Return true if all dbs named in dbHash are present in ma->components 
 * (ma->components is assumed not to have any duplicate src's). */
{
struct mafComp *mc = NULL;
int numInteresting = 0;
for (mc = ma->components;  mc != NULL;  mc = mc->next)
    {
    char *seq = cloneString(mc->src);
    char *ptr = strchr(seq, '.');
    if (ptr != NULL)
	*ptr = 0;
    if (hashLookup(dbHash, seq) != NULL)
	numInteresting++;
    freez(&seq);
    }
return (numInteresting == dbHash->elCount);
}


void mafRangesSimple(struct mafAli *ma, char *dbDot, int dbDotLen, FILE *f)
/* Output the whole range for the species of interest. */
{
struct mafComp *mc = NULL;
for (mc = ma->components;  mc != NULL;  mc = mc->next)
    {
    if (mc->src != NULL && startsWith(dbDot, mc->src))
	{
	int start = mc->start;
	int end = start + mc->size;
	if (mc->strand == '-')
	    reverseIntRange(&start, &end, mc->srcSize);
	fprintf(f, "%s\t%d\t%d\n", mc->src+dbDotLen, start, end);
	break;
	}
    }
}

void mafRangesMindGaps(struct mafAli *ma, char *dbDot, int dbDotLen, FILE *f)
/* Output coverage range for db, but break it up into separate ranges 
 * around bases where all otherDbs have gaps. */
{
struct mafComp *mc = NULL;
char *chrom = NULL;
int tStart = 0, tPos = 0;
int tSize = 0, tEnd = 0, tSrcSize = 0;
char tStrand = '.';
int i = 0;

/* First, loop through components to find "target" (db) to initialize state 
 * and find out if its strand is - and we need to step through sequences 
 * backwards. */
for (mc = ma->components;  mc != NULL;  mc = mc->next)
    {
    if (mc->src != NULL && startsWith(dbDot, mc->src))
	{
	/* Initialize target state: */
	chrom = mc->src + dbDotLen;
	tStart = mc->start;
	tSize = mc->size;
	tEnd = tStart + tSize;
	tStrand = mc->strand;
	tSrcSize = mc->srcSize;
	if (tStrand == '-')
	    reverseIntRange(&tStart, &tEnd, tSrcSize);
	tPos = tStart;
	}
    }

/* outer loop: positions in alignment.  inner loop: sequences in alignment. */
for (i = 0;  i < ma->textSize;  i++)
    {
    boolean tGap = FALSE;
    boolean qGap = TRUE;
    int j = (tStrand == '-' ? (ma->textSize - i - 1) : i);

    for (mc = ma->components;  mc != NULL;  mc = mc->next)
	{
	if (mc->src != NULL && startsWith(dbDot, mc->src))
	    {
	    if (mc->text[j] == '-')
		tGap = TRUE;
	    }
	else
	    {
	    if (mc->text[j] != '-')
		qGap = FALSE;
	    }
	}
    /* Only act if target doesn't contain a gap here. */
    if (! tGap)
	{
	if (qGap)
	    {
	    if (tStart < tPos)
		fprintf(f, "%s\t%d\t%d\n", chrom, tStart, tPos);
	    tStart = tPos + 1;
	    }
	tPos++;
	}
    }
if (tStart < tPos)
    fprintf(f, "%s\t%d\t%d\n", chrom, tStart, tPos);
if (tPos != tEnd)
    {
    tStart = tEnd - tSize;
    if (tStrand == '-')
	{
	reverseIntRange(&tStart, &tEnd, tSrcSize);
	tPos = tSrcSize - tPos;
	}
    errAbort("Error: %s.%s %d %d %c %d: ended up at %d instead of %d\n",
	     dbDot, chrom, tStart, tSize, tStrand, tSrcSize, tPos, tEnd);
    }
}


void mafRanges(char *in, char *db, char *out)
/* mafRanges - Extract ranges of target (or query) coverage from maf and 
 * output as BED 3 (intended for further processing by featureBits). */
{
struct mafFile *mf = mafOpen(in);
FILE *f = mustOpen(out, "w");
struct mafAli *ma = NULL;
struct hash *dbHash = hashDbs(db, otherDb);
char *dbDot = (char *)cloneMem(db, strlen(db)+2);
int dbDotLen = 0;

strcat(dbDot, ".");
dbDotLen = strlen(dbDot);
while ((ma = mafNext(mf)) != NULL)
    {
    if (gotDbs(ma, dbHash))
	{
	if (notAllOGap)
	    mafRangesMindGaps(ma, dbDot, dbDotLen, f);
	else
	    mafRangesSimple(ma, dbDot, dbDotLen, f);
	}
    mafAliFree(&ma);
    }

mafFileFree(&mf);
fclose(f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
otherDb = optionVal("otherDb", otherDb);
notAllOGap = optionExists("notAllOGap");
if (argc != 4)
    usage();
mafRanges(argv[1], argv[2], argv[3]);
return 0;
}
