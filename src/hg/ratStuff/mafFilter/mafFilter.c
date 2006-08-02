/* mafFilter - Filter out maf files. */
#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "maf.h"

static char const rcsid[] = "$Id: mafFilter.c,v 1.13 2006/08/02 23:59:42 kate Exp $";

#define DEFAULT_MIN_ROW 2
#define DEFAULT_MIN_COL 1
#define DEFAULT_FACTOR 5

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafFilter - Filter out maf files. Output goes to standard out\n"
  "usage:\n"
  "   mafFilter file(s).maf\n"
  "options:\n"
  "   -tolerate - Just ignore bad input rather than aborting.\n"
  "   -minCol=N - Filter out blocks with fewer than N columns (default %d)\n"
  "   -minRow=N - Filter out blocks with fewer than N rows (default %d)\n"
  "   -factor - Filter out scores below -minFactor * (ncol**2) * nrow\n"
        /* score cutoff recommended by Webb Miller */
  "   -minFactor=N - Factor to use with -minFactor (default %d)\n"
  "   -minScore=N - Minimum allowed score (alternative to -minFactor)\n"
  "   -reject=filename - Save rejected blocks in filename\n"
  "   -needComp=species - all alignments must have species as one of the component\n"
  "   -overlap - Reject overlapping blocks in reference (assumes ordered blocks)\n"
  "   -componentFilter=filename - Filter out blocks without a component listed in filename \n",
        DEFAULT_MIN_COL, DEFAULT_MIN_ROW, DEFAULT_FACTOR
  );
}

static struct optionSpec options[] = {
   {"tolerate", OPTION_BOOLEAN},
   {"minCol", OPTION_INT},
   {"minRow", OPTION_INT},
   {"minScore", OPTION_FLOAT},
   {"factor", OPTION_BOOLEAN},
   {"minFactor", OPTION_INT},
   {"reject", OPTION_STRING},
   {"componentFilter", OPTION_STRING},
   {"needComp", OPTION_STRING},
   {"overlap", OPTION_BOOLEAN},
   {NULL, 0},
};

/* record number of rejected blocks, and reason */
int rejectMinCol = 0;
int rejectMinRow = 0;
int rejectMinScore = 0;
int rejectMinFactor = 0;
int rejectNeedComp = 0;
int rejectComponentFilter = 0;
int rejectOverlap = 0;

int minCol = DEFAULT_MIN_COL;
int minRow = DEFAULT_MIN_ROW;
boolean gotMinScore = FALSE;
double minScore;
boolean gotMinFactor = FALSE;
int minFactor = DEFAULT_FACTOR;
char *componentFile = NULL;
struct hash *cHash = NULL;
char *rejectFile = NULL;
char *needComp = NULL;

static jmp_buf recover;    /* Catch errors in load maf the hard way in C. */

static void abortHandler()
/* Abort fuzzy finding. */
{
if (optionExists("tolerate"))
    {
    longjmp(recover, -1);
    }
else
    exit(-1);
}

struct hash *hashComponentList(char *fileName)
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[1];
struct hash *cHash = newHash(0);
while (lineFileRow(lf, row))
    {
    char *name = row[0];
    hashAdd(cHash, name, name);
    }
lineFileClose(&lf);
return cHash;
}

boolean filterOne(struct mafAli *maf)
/* Return TRUE if maf passes filters. */
{
int ncol = maf->textSize;
int nrow = slCount(maf->components);
double ncol2 = ncol * ncol;
double fscore = -minFactor * ncol2 * nrow;
char *ref = maf->components->src;
/* for overlap detection */
static char *prevRef = NULL;
static int prevRefStart = 0, prevRefEnd = 0;
int refStart = maf->components->start;
int refEnd = refStart + maf->components->size;

if (needComp && (mafMayFindCompPrefix(maf, needComp, "." ) == NULL))
    {
    verbose(3, "%s:%d needComp\n", 
            maf->components->src, maf->components->start);
    rejectNeedComp++;
    return FALSE;
    }

if (componentFile != NULL && (mafMayFindComponentInHash(maf, cHash) == NULL))
    {
    verbose(3, "%s:%d componentFilter\n", 
                maf->components->src, maf->components->start);
    rejectComponentFilter++;
    return FALSE;
    }
if (nrow < minRow)
    {
    verbose(3, "%s:%d nrow=%d\n", maf->components->src, 
                    maf->components->start, nrow);
    rejectMinRow++;
    return FALSE;
    }
if (ncol < minCol)
    {
    verbose(3, "%s:%d ncol=%d\n", maf->components->src, 
                    maf->components->start, ncol);
    rejectMinCol++;
    return FALSE;
    }
if (gotMinScore && maf->score < minScore)
    {
    verbose(3, "%s:%d score=%f\n", maf->components->src, 
                    maf->components->start, maf->score);
    rejectMinScore++;
    return FALSE;
    }
if (gotMinFactor && maf->score < fscore)
    {
    verbose(3, "%s:%d fscore=%f\n", maf->components->src, 
                    maf->components->start, fscore);
    rejectMinFactor++;
    return FALSE;
    }
if (optionExists("overlap"))
    {
    if (prevRef && sameString(ref, prevRef) &&
            refStart < prevRefEnd && refEnd > prevRefStart)
        {
        verbose(3, "%s:%d overlap %d\n", maf->components->src, 
                maf->components->start, prevRefStart);
        rejectOverlap++;
        return FALSE;
        }
    if (!prevRef || !sameString(ref, prevRef))
	{
	freeMem(prevRef);
	prevRef = cloneString(ref);
	}
    prevRefStart = refStart;
    prevRefEnd = refEnd;
    }
return TRUE;
}

void mafFilter(int fileCount, char *files[])
/* mafFilter - Filter out maf files. */
{
FILE *f = stdout;
FILE *rf = NULL;
int i;
int status;
int rejects = 0;
int categorizedRejects = 0;

for (i=0; i<fileCount; ++i)
    {
    char *fileName = files[i];
    struct mafFile *mf = mafOpen(fileName);
    struct mafAli *maf = NULL;
    if (i == 0)
        {
        mafWriteStart(f, mf->scoring);
        if (rejectFile && rf == NULL)
            {
            rf = mustOpen(rejectFile, "w");
            mafWriteStart(rf, mf->scoring);
            }
        }
    for (;;)
	{
	/* Effectively wrap try/catch around mafNext() */
	status = setjmp(recover);
	if (status == 0)    /* Always true except after long jump. */
	    {
	    pushAbortHandler(abortHandler);
	    maf = mafNext(mf);
	    if (maf != NULL)
                {
                if (filterOne(maf))
                    mafWrite(f, maf);
                else 
                    {
                    rejects++;
                    if (rf)
                        mafWrite(rf, maf);
                    }
                }
	    }
	popAbortHandler();
	if (maf == NULL)
	    break;
	mafAliFree(&maf);
	}
    mafFileFree(&mf);
    }
if (rejectMinCol)
    fprintf(stderr, "rejected minCol: %d\n", rejectMinCol);
if ( rejectMinRow)
    fprintf(stderr, "rejected minRow: %d\n", rejectMinRow);
if ( rejectMinScore)
    fprintf(stderr, "rejected minScore: %d\n", rejectMinScore);
if (rejectMinFactor)
    fprintf(stderr, "rejected minFactor: %d\n", rejectMinFactor);
if (rejectNeedComp)
    fprintf(stderr, "rejected needComp: %d\n", rejectNeedComp);
if (rejectComponentFilter)
    fprintf(stderr, "rejected componentFilter: %d\n", rejectComponentFilter);
if ( rejectOverlap)
    fprintf(stderr, "rejected overlap: %d\n", rejectOverlap);
categorizedRejects = rejectMinCol + rejectMinRow + rejectMinScore +
                     rejectMinFactor + rejectNeedComp + rejectComponentFilter +
                     rejectOverlap;
if (rejects != categorizedRejects)
    fprintf(stderr, "uncategorized rejects: %d\n",rejects - categorizedRejects);
fprintf(stderr, "total rejected: %d blocks\n", rejects);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 2)
    usage();
if (optionExists("minScore"))
    {
    gotMinScore = TRUE;
    minScore = optionFloat("minScore", 0);
    }
if (optionExists("factor"))
    {
    if (gotMinScore)
        errAbort("can't use both -minScore and -minFactor");
    gotMinFactor = TRUE;
    minFactor = optionInt("minFactor", DEFAULT_FACTOR);
    }
minCol = optionInt("minCol", DEFAULT_MIN_COL);
minRow = optionInt("minRow", DEFAULT_MIN_ROW);
componentFile = optionVal("componentFilter", NULL);
if (componentFile != NULL)
    cHash = hashComponentList(componentFile);
rejectFile = optionVal("reject", NULL);
needComp = optionVal("needComp", NULL);
verbose(3, "minCol=%d, minRow=%d, gotMinScore=%d, minScore=%f, gotMinFactor=%d, minFactor=%d\n", 
        minCol, minRow, gotMinScore, minScore, gotMinFactor, minFactor);

mafFilter(argc-1, argv+1);
return 0;
}
