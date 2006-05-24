/* hgLoadMaf - Load a maf file index into the database. */
#include "common.h"
#include "cheapcgi.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "hdb.h"
#include "hgRelate.h"
#include "portable.h"
#include "maf.h"
#include "scoredRef.h"
#include "dystring.h"

static char const rcsid[] = "$Id: hgLoadMaf.c,v 1.18 2006/05/01 16:33:35 angie Exp $";

/* Command line options */

/* multiz currently will generate some alignments that have a missing score
 * and/or contain only 1 or 2 sequences.  Webb Miller recommends these
 * be ignored, and he intends to remove them from multiz output.
 * When/if this happens, the -warn option should not be used.
 */

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"warn", OPTION_BOOLEAN},
    {"WARN", OPTION_BOOLEAN},
    {"test", OPTION_STRING},
    {"pathPrefix", OPTION_STRING},
    {NULL, 0}
};
boolean warnOption = FALSE;
boolean warnVerboseOption = FALSE;     /* print warning detail */
char *testFile = NULL;                    /* maf filename if testing */
char *pathPrefix = NULL;               /* extFile directory */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadMaf - Load a maf file index into the database\n"
  "usage:\n"
  "   hgLoadMaf database table\n"
  "options:\n"
    "   -warn   warn instead of error if empty/incomplete alignments\n"
    "           are found found.\n"
    "   -WARN   warn instead of error, with detail for the warning\n" 
    "   -test=infile  use infile as input, and suppress loading\n"
"                     the database. Just create .tab file in current dir.\n" 
    "   -pathPrefix=dir  load files from specified directory (default /gbdb/database/table.\n"
  "The maf files need to already exist in chromosome coordinates.\n"
  );
}

struct mafComp *findComponent(struct mafAli *maf, char *prefix)
/* Find maf component with the given prefix. */
{
int preLen = strlen(prefix);
struct mafComp *mc;
for (mc = maf->components; mc != NULL; mc = mc->next)
    {
    if (memcmp(mc->src, prefix, preLen) == 0 && mc->src[preLen] == '.')
        return mc;
    }
return NULL;
}

void hgLoadMaf(char *database, char *table)
/* hgLoadMaf - Load a maf file index into the database. */
{
struct fileInfo *fileList = NULL, *fileEl;
struct sqlConnection *conn;
long mafCount = 0;
FILE *f = hgCreateTabFile(".", table);
char extFileDir[512];
char ext[10];
char file[100];

if (testFile != NULL)
    {
    if (!fileExists(testFile))
        errAbort("Test file %s doesn't exist\n", testFile);
    splitPath(testFile, extFileDir, file, ext);
    strcat(file, ext);
    fileList = listDirX(extFileDir, file, TRUE);
    pathPrefix = extFileDir;
    }
else
    {
    hSetDb(database);
    if (pathPrefix == NULL)
        {
        safef(extFileDir, sizeof(extFileDir), "/gbdb/%s/%s", database, table);
        pathPrefix = extFileDir;
        }
    fileList = listDirX(pathPrefix, "*.maf", TRUE);
    conn = hgStartUpdate();
    scoredRefTableCreate(conn, table, hGetMinIndexLength());
    }
if (fileList == NULL)
    errAbort("%s doesn't exist or doesn't have any maf files", pathPrefix);
for (fileEl = fileList; fileEl != NULL; fileEl = fileEl->next)
    {
    char *fileName = fileEl->name;
    struct mafFile *mf;
    struct mafComp *mc;
    struct scoredRef mr;
    struct mafAli *maf;
    off_t offset;
    int warnCount = 0;
    int dbNameLen = strlen(database);
    HGID extId;
    if (testFile != NULL)
        extId = 0;
    else
        extId = hgAddToExtFile(fileName, conn);

    mf = mafOpen(fileName);
    verbose(1, "Indexing and tabulating %s\n", fileName);
    while ((maf = mafNextWithPos(mf, &offset)) != NULL)
        {
	double maxScore, minScore;
	mc = findComponent(maf, database);
	if (mc == NULL) 
            {
            char msg[256];
            safef(msg, sizeof(msg),
                            "Couldn't find %s. sequence line %d of %s\n", 
	    	                database, mf->lf->lineIx, fileName);
            if (warnOption || warnVerboseOption) 
                {
                warnCount++;
                if (warnVerboseOption)
                    verbose(1, msg);
                mafAliFree(&maf);
                continue;
                }
            else 
                errAbort(msg);
            }
	ZeroVar(&mr);
	mr.chrom = mc->src + dbNameLen + 1;
	mr.chromStart = mc->start;
	mr.chromEnd = mc->start + mc->size;
	if (mc->strand == '-')
	    reverseIntRange(&mr.chromStart, &mr.chromEnd, mc->srcSize);
	mr.extFile = extId;
	mr.offset = offset;

        /* The maf scores are the sum of all pairwise
         * alignment scores.  If you have A,B,C,D
         * species in the alignments it's the sum of
         * the AB, AC, AD, BC, BD, CD pairwise scores.
         * If you just had two species it would just
         * be a single pairwise score.  Since we don't
         * want to penalize things for missing species,
         * we basically divide by the number of possible
         * pairings.  To get it from the -100 to +100 (more or
         * less) per base blastz scoring scheme to something
         * that goes from 0 to 1, we also also divide by 200.
         * JK */

        /* scale using alignment size to get per-base score */
	mr.score = maf->score/mc->size;

        /* get scoring range -- depends on #species in alignment */
        mafColMinMaxScore(maf, &minScore, &maxScore);
        if (maxScore == minScore) 
            /* protect against degenerate case -- 1 species, produces
             * max & min score of 0 */
            mr.score = 0.0;
        else
            /* translate to get zero-based score, 
             * then scale by blastz scoring range */
            mr.score = (mr.score-minScore)/(maxScore-minScore);
	if (mr.score <= 0.0) 
            {
            char msg[256];
            safef(msg, sizeof(msg),
                    "Score too small (raw %.1f scaled %.1f #species %d),"
                           " line %d of %s\n", 
                        maf->score, mr.score, slCount(maf->components),
                                mf->lf->lineIx, fileName);
            if (warnOption || warnVerboseOption) 
                {
                warnCount++;
                if (warnVerboseOption)
                    verbose(1, msg);
                }
	    mr.score = 0.001;
            }
	if (mr.score > 1.0) mr.score = 1.0;
	mafCount++;
	fprintf(f, "%u\t", hFindBin(mr.chromStart, mr.chromEnd));
	scoredRefTabOut(&mr, f);
	mafAliFree(&maf);
	}
    mafFileFree(&mf);
    if (warnCount)
        verbose(1, "%d warnings\n", warnCount);
    }
if (testFile != NULL)
    return;
verbose(1, "Loading %s into database\n", table);
hgLoadTabFile(conn, ".", table, &f);
verbose(1, "Loaded %ld mafs in %d files from %s\n", mafCount, slCount(fileList), pathPrefix);
hgEndUpdate(&conn, "Add %ld mafs in %d files from %s\n", mafCount, slCount(fileList), pathPrefix);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
warnOption = optionExists("warn");
warnVerboseOption = optionExists("WARN");
testFile = optionVal("test", NULL);
pathPrefix = optionVal("pathPrefix", NULL);
if (argc != 3)
    usage();
hgLoadMaf(argv[1], argv[2]);
return 0;
}
